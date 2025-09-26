#include "spfm.h"
#include "error.h"
#include "util.h"
#include "ftd2xx.h"
#include "chiptype.h"
#include "ym2151.h"
#include "ym2612.h"
#include "ym2203.h"
#include "ym2413.h"
#include "sn76489.h"
#include "ay8910.h"
#include "y8950.h"
#include "ym3526.h"
#include "ym3812.h"
#include "ymf262.h"
#include "ym2608.h"
#include <string.h>
#include <stdio.h>

static FT_HANDLE ftHandle = NULL;
static SPFM_TYPE spfm_type = SPFM_TYPE_UNKNOWN;
static int g_selected_dev_idx = -1;

static uint8_t spfm_write_buf[SPFM_WRITE_BUF_SIZE];
static DWORD spfm_write_buf_ptr = 0;

static bool spfm_identify();

// New function to handle waiting and writing
void spfm_wait_and_write_reg(uint32_t wait_samples, uint8_t slot, uint8_t port, uint8_t addr, uint8_t data) {
    // Threshold for using hardware waits (in samples).
    // Based on analysis, hardware waits are precise for very short durations.
    const uint32_t HW_WAIT_THRESHOLD = 10;

    if (wait_samples > 0) {
        // SPFM_Light supports precise, low-CPU hardware waits for single samples (0x80 command).
        // This is ideal for very short delays.
        if (spfm_type == SPFM_TYPE_SPFM_LIGHT && wait_samples < HW_WAIT_THRESHOLD) {
            if (spfm_write_buf_ptr + wait_samples > SPFM_WRITE_BUF_SIZE) {
                spfm_flush();
            }
            for (uint32_t i = 0; i < wait_samples; i++) {
                spfm_write_buf[spfm_write_buf_ptr++] = 0x80;
            }
        } else {
            // For longer delays or other devices, use the high-resolution software timer.
            // This keeps CPU usage low while maintaining good accuracy.
            unsigned int wait_us = (unsigned int)(((uint64_t)wait_samples * 1000000) / 44100);
            if (wait_us > 0) {
                spfm_flush(); // Ensure all previous commands are sent before waiting.
                yasp_usleep(wait_us);
            }
        }
    }

    // A zero address and data is treated as a wait-only command and no register write will be sent.
    if (addr == 0 && data == 0 && port == 0) {
        return;
    }
    spfm_write_reg(slot, port, addr, data);
}


bool spfm_flush(void) {
    DWORD total_written = 0;
    DWORD bytes_to_write = 0;
    DWORD bytes_written_this_chunk = 0;
    FT_STATUS ftStatus;

    if (!ftHandle || spfm_write_buf_ptr == 0) {
        return true;
    }

    while (total_written < spfm_write_buf_ptr) {
        bytes_to_write = spfm_write_buf_ptr - total_written;
        if (bytes_to_write > 4096) {
            bytes_to_write = 4096;
        }

        ftStatus = FT_Write(ftHandle, spfm_write_buf + total_written, bytes_to_write, &bytes_written_this_chunk);

        if (ftStatus != FT_OK) {
            logging(LOG_ERROR, "FT_Write failed in spfm_flush. FT_Status=%d\n", (int)ftStatus);
            spfm_write_buf_ptr = 0; // Clear buffer on error
            return false;
        }

        if (bytes_written_this_chunk != bytes_to_write) {
            logging(LOG_ERROR, "SPFM flush failed. Wrote %lu of %lu bytes in a chunk.\n", bytes_written_this_chunk, bytes_to_write);
            spfm_write_buf_ptr = 0;
            return false;
        }

        total_written += bytes_written_this_chunk;
    }

    spfm_write_buf_ptr = 0;
    return true;
}


int spfm_init(int dev_idx) {
    FT_STATUS ftStatus;
    g_selected_dev_idx = dev_idx;

    ftStatus = FT_Open(dev_idx, &ftHandle);
    if (ftStatus != FT_OK) {
        logging(LOG_ERROR, "FT_Open failed for device index %d, error code: %d\n", dev_idx, (int)ftStatus);
        return -1;
    }

    // Configure and try to identify
    FT_SetBaudRate(ftHandle, 1500000);
    FT_SetDataCharacteristics(ftHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
    FT_SetFlowControl(ftHandle, FT_FLOW_NONE, 0, 0);
    FT_SetTimeouts(ftHandle, 100, 100); // Increase timeout for identification
    FT_SetLatencyTimer(ftHandle, 2);
    FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);
    FT_SetDtr(ftHandle);
    FT_SetRts(ftHandle);
    yasp_usleep(100);
    FT_ClrDtr(ftHandle);
    FT_ClrRts(ftHandle);
    yasp_usleep(100);
    FT_SetRts(ftHandle);
    yasp_usleep(100);
    FT_ClrDtr(ftHandle);
    FT_ClrRts(ftHandle);
    yasp_usleep(100);

    // HACK: Bypass identification as per user feedback. Assume SPFM_Light.
    // The spfm_identify() function seems to fail on this specific hardware setup,
    // even though the device is correctly enumerated. By skipping this step,
    // we avoid the handshake that causes the failure.
    logging(WARN, "Bypassing SPFM identification. Assuming device is SPFM_Light.\n");
    spfm_type = SPFM_TYPE_SPFM_LIGHT;
    /*
    if (!spfm_identify()) {
        logging(LOG_ERROR, "Could not identify SPFM device at index %d.\n", dev_idx);
        FT_Close(ftHandle);
        ftHandle = NULL;
        return -1;
    }
    */

    // Re-configure timeouts for playback. A slightly longer read timeout improves stability.
    FT_SetTimeouts(ftHandle, 100, 100);
    // Set a larger USB transfer buffer, similar to node-spfm, to improve bulk write performance.
    // 64KB is a common and safe maximum for D2XX.
    FT_SetUSBParameters(ftHandle, 65536, 65536);
    logging(INFO, "SPFM device at index %d initialized successfully. Type: %s\n", dev_idx, spfm_type == SPFM_TYPE_SPFM_LIGHT ? "SPFM_Light" : "SPFM");
    
    return 0;
}

void spfm_init_chips() {
    // Initialize chips based on the global configuration
    DWORD i;
    for (i = 0; i < CHIP_TYPE_COUNT; i++) {
        if (g_chip_config[i].type != CHIP_TYPE_NONE && g_chip_config[i].slot != 0xFF) {
            logging(INFO, "Initializing chip type %d in slot %d\n", g_chip_config[i].type, g_chip_config[i].slot);
            switch (g_chip_config[i].type) {
                case CHIP_TYPE_YM2608:
                    ym2608_init(g_chip_config[i].slot);
                    break;
                case CHIP_TYPE_YM2151:
                    ym2151_init(g_chip_config[i].slot);
                    break;
                case CHIP_TYPE_YM2612:
                    ym2612_init(g_chip_config[i].slot);
                    break;
                case CHIP_TYPE_YM2203:
                    ym2203_init(g_chip_config[i].slot);
                    break;
                case CHIP_TYPE_YM2413:
                    ym2413_init(g_chip_config[i].slot);
                    break;
                case CHIP_TYPE_SN76489:
                    sn76489_init(g_chip_config[i].slot);
                    break;
                case CHIP_TYPE_AY8910:
                    ay8910_init(g_chip_config[i].slot);
                    break;
                case CHIP_TYPE_Y8950:
                    y8950_init(g_chip_config[i].slot);
                    break;
                case CHIP_TYPE_YM3526:
                    ym3526_init(g_chip_config[i].slot);
                    break;
                case CHIP_TYPE_YM3812:
                    ym3812_init(g_chip_config[i].slot);
                    break;
                case CHIP_TYPE_YMF262:
                    ymf262_init(g_chip_config[i].slot);
                    break;
                default:
                    // Do nothing for unhandled or NONE types
                    break;
            }
        }
    }

    spfm_flush();
}

static bool spfm_identify() {
    DWORD bytesWritten, bytesRead;
    uint8_t write_buf[1] = { 0xFF };
    char read_buf[3] = {0};
    FT_STATUS ftStatus;
    uint8_t reset_cmd[] = { 0xFE };
    int outer_retries = 3; // Add an outer loop for more robustness

    for (int i = 0; i < outer_retries; i++) {
        FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);
        
        ftStatus = FT_Write(ftHandle, write_buf, 1, &bytesWritten);
        if (ftStatus != FT_OK || bytesWritten != 1) {
            yasp_usleep(50000); // Shorter wait
            continue;
        }

        yasp_usleep(100000); // Wait 100ms for a response

        ftStatus = FT_Read(ftHandle, read_buf, 2, &bytesRead);
        if (ftStatus == FT_OK && bytesRead >= 2) {
            if (strncmp(read_buf, "LT", 2) == 0) {
                spfm_type = SPFM_TYPE_SPFM_LIGHT;
                ftStatus = FT_Write(ftHandle, reset_cmd, sizeof(reset_cmd), &bytesWritten);
                if (ftStatus != FT_OK) {
                    logging(WARN, "FT_Write for SPFM_Light reset failed with status %d\n", (int)ftStatus);
                }
                return true;
            } else if (strncmp(read_buf, "OK", 2) == 0) {
                spfm_type = SPFM_TYPE_SPFM;
                return true;
            }
        }
        yasp_usleep(50000); // Shorter wait before next outer retry
    }
    
    return false;
}


void spfm_chip_reset() {
    int i;
    uint8_t slot;

    // Reset all configured chips
    for (i = 0; i < CHIP_TYPE_COUNT; i++) {
        if (g_chip_config[i].type != CHIP_TYPE_NONE && g_chip_config[i].slot != 0xFF) {
            slot = g_chip_config[i].slot;
            logging(INFO, "Resetting chip type %d in slot %d\n", g_chip_config[i].type, slot);
            switch (g_chip_config[i].type) {
                case CHIP_TYPE_YM2608:
                    ym2608_mute(slot);
                    break;
                case CHIP_TYPE_YM2151:
                    ym2151_mute(slot);
                    break;
                case CHIP_TYPE_YM2612:
                    ym2612_mute(slot);
                    break;
                case CHIP_TYPE_YM2203:
                    ym2203_mute(slot);
                    break;
                case CHIP_TYPE_YM2413:
                    ym2413_mute(slot);
                    break;
                case CHIP_TYPE_SN76489:
                    sn76489_mute(slot);
                    break;
                case CHIP_TYPE_AY8910:
                    ay8910_mute(slot);
                    break;
                case CHIP_TYPE_Y8950:
                    y8950_mute(slot);
                    break;
                case CHIP_TYPE_YM3526:
                    ym3526_mute(slot);
                    break;
                case CHIP_TYPE_YM3812:
                    ym3812_mute(slot);
                    break;
                case CHIP_TYPE_YMF262:
                    ymf262_mute(slot);
                    break;
                default:
                    break;
            }
        }
    }
    spfm_flush();
}

void spfm_cleanup() {
    if (ftHandle) {
        spfm_chip_reset();
        spfm_reset();
        FT_Close(ftHandle);
        ftHandle = NULL;
        logging(INFO, "SPFM device closed.\n");
    }
}

FT_HANDLE spfm_get_handle() {
    return ftHandle;
}

SPFM_TYPE spfm_get_type() {
    return spfm_type;
}

void spfm_reset() {
    if (!ftHandle) return;
    spfm_flush();
    uint8_t cmd_buf[1];
    if (spfm_type == SPFM_TYPE_SPFM_LIGHT) {
        cmd_buf[0] = 0xFE;
        spfm_write_reg(0, 0, cmd_buf[0], 0);
    } else if (spfm_type == SPFM_TYPE_SPFM) {
        cmd_buf[0] = 0xFF;
        spfm_write_reg(0, 0, cmd_buf[0], 0);
    }
    spfm_flush();
}

void spfm_write_reg(uint8_t slot, uint8_t port, uint8_t addr, uint8_t data) {
    if (!ftHandle) return;

    size_t cmd_size = 0;
    uint8_t cmd_buf[4];

    if (spfm_type == SPFM_TYPE_SPFM_LIGHT) {
        cmd_buf[0] = slot & 1;
        cmd_buf[1] = (port & 7) << 1;
        cmd_buf[2] = addr;
        cmd_buf[3] = data;
        cmd_size = 4;
    } else if (spfm_type == SPFM_TYPE_SPFM) {
        cmd_buf[0] = ((slot & 7) << 4) | (port & 3);
        cmd_buf[1] = addr;
        cmd_buf[2] = data;
        cmd_size = 3;
    } else {
        return;
    }

    if (spfm_write_buf_ptr + cmd_size > SPFM_WRITE_BUF_SIZE) {
        spfm_flush();
    }

    memcpy(spfm_write_buf + spfm_write_buf_ptr, cmd_buf, cmd_size);
    spfm_write_buf_ptr += cmd_size;
}

void spfm_write_regs(uint8_t slot, const spfm_reg_t* regs, uint32_t count, uint32_t write_wait) {
    if (!ftHandle) return;

    for (uint32_t i = 0; i < count; i++) {
        spfm_write_reg(slot, regs[i].port, regs[i].addr, regs[i].data);
        if (write_wait > 0) {
            if (spfm_write_buf_ptr + write_wait > SPFM_WRITE_BUF_SIZE) {
                spfm_flush();
            }
            for (uint32_t j = 0; j < write_wait; j++) {
                spfm_write_buf[spfm_write_buf_ptr++] = 0x80;
            }
        }
    }
}

void spfm_write_data(uint8_t slot, uint8_t data) {
    if (!ftHandle) return;

    size_t cmd_size = 0;
    uint8_t cmd_buf[3];

    if (spfm_type == SPFM_TYPE_SPFM_LIGHT) {
        cmd_buf[0] = slot & 1;
        cmd_buf[1] = 0x20; // Simplified command for data-only writes (e.g., SN76489)
        cmd_buf[2] = data;
        cmd_size = 3;
    } else if (spfm_type == SPFM_TYPE_SPFM) {
        // SPFM (standard) does not support this simplified command.
        // Fallback to a regular write with dummy address/port, though this is not ideal.
        // The primary use case is for SPFM_Light with SN76489.
        spfm_write_reg(slot, 0, 0, data);
        return;
    } else {
        return;
    }

    if (spfm_write_buf_ptr + cmd_size > SPFM_WRITE_BUF_SIZE) {
        spfm_flush();
    }

    memcpy(spfm_write_buf + spfm_write_buf_ptr, cmd_buf, cmd_size);
    spfm_write_buf_ptr += cmd_size;
}

int spfm_get_selected_device_index(void) {
    return g_selected_dev_idx;
}

void spfm_write_ym2608_ram(uint8_t slot, uint32_t address, uint32_t size, const uint8_t* data) {
    uint32_t start = address;
    uint32_t stop = start + size - 1;
    // The limit seems to be the end of the ADPCM-A RAM area.
    uint32_t limit = (stop < 0x3FFFF) ? stop : 0x3FFFF; 
    uint32_t i;
    const uint32_t chunk_size = 256;

    if (!ftHandle) return;

    // Crucial step from node-spfm: addresses are shifted.
    start >>= 2;
    stop >>= 2;
    limit >>= 2;

    // Setup memory write mode, matching node-spfm
    spfm_write_reg(slot, 1, 0x10, 0x80); // Reset Flags
    spfm_write_reg(slot, 1, 0x00, 0x60); // Memory Write command
    spfm_write_reg(slot, 1, 0x01, 0x00); // Memory Type

    // Set addresses
    spfm_write_reg(slot, 1, 0x02, start & 0xff);
    spfm_write_reg(slot, 1, 0x03, (start >> 8) & 0xff);
    spfm_write_reg(slot, 1, 0x04, stop & 0xff);
    spfm_write_reg(slot, 1, 0x05, (stop >> 8) & 0xff);
    spfm_write_reg(slot, 1, 0x0c, limit & 0xff);
    spfm_write_reg(slot, 1, 0x0d, (limit >> 8) & 0xff);
    
    // Write data in chunks. The flush at the end of the loop provides
    // the necessary flow control, equivalent to 'await' in the TS code.
    for (i = 0; i < size; i++) {
        spfm_write_reg(slot, 1, 0x08, data[i]);
        if ((i + 1) % chunk_size == 0) {
            spfm_flush(); 
        }
    }

    // End memory write mode
    spfm_write_reg(slot, 1, 0x00, 0x00);
    spfm_write_reg(slot, 1, 0x10, 0x80); // Reset Flags again
    spfm_flush(); // Flush the final commands
}
