#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "s98.h"
#include "spfm.h"
#include "util.h"
#include "error.h"
#include "play.h"
#include "chiptype.h"

#define S98_SYNC_NTSC 1
#define S98_SYNC_PAL 2

static bool s98_play_loop(S98* s98, int timer_mode, const char *filename);
static uint32_t s98_get_val(S98* s98);

bool s98_load(S98* s98, FILE* fp) {
    if (!fp) {
        return false;
    }

    fseek(fp, 0, SEEK_END);
    s98->size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    s98->buffer = (uint8_t*)malloc(s98->size);
    if (!s98->buffer) {
        fclose(fp);
        return false;
    }

    if (fread(s98->buffer, 1, s98->size, fp) != s98->size) {
        free(s98->buffer);
        s98->buffer = NULL;
        return false;
    }

    return true;
}

bool s98_parse_header(S98* s98) {
    if (s98->size < 0x20 || memcmp(s98->buffer, "S98", 3) != 0) {
        return false;
    }
    s98->version = s98->buffer[3];
    s98->timer_info = read_le32(s98->buffer + 4);
    s98->timer_info2 = read_le32(s98->buffer + 8);
    s98->compressing = read_le32(s98->buffer + 12);
    s98->offset_to_dump = read_le32(s98->buffer + 16);
    s98->offset_to_loop = read_le32(s98->buffer + 20);
    s98->device_count = read_le32(s98->buffer + 24); // S98v3 only

    s98->pos = s98->offset_to_dump;
    return true;
}

bool s98_play(S98* s98, const char *filename) {
    if (!s98_parse_header(s98)) {
        logging(LOG_ERROR, "S98 header parse error.\n");
        return false;
    }
    int timer_mode = S98_SYNC_NTSC;
    if (s98->timer_info != 0 && s98->timer_info2 != 0) {
        if (s98->timer_info2 == 198) { // A common PAL value
            timer_mode = S98_SYNC_PAL;
        }
    }
    return s98_play_loop(s98, timer_mode, filename);
}

void s98_release(S98* s98) {
    if (s98 && s98->buffer) {
        free(s98->buffer);
        s98->buffer = NULL;
    }
}

static bool s98_play_loop(S98* s98, int timer_mode, const char *filename) {
    uint32_t sync_count = 0;
    uint32_t sync_wait = (timer_mode == S98_SYNC_PAL) ? 20000 : 10000;
    extern volatile bool g_is_paused, g_next_track_flag, g_prev_track_flag, g_random_mode;

    // S98 doesn't have a total time in the header, so we pass 0.
    update_ui(0, 0, filename, false, g_random_mode);

    while (g_is_playing && s98->pos < s98->size && !g_next_track_flag && !g_prev_track_flag) {
        while (g_is_paused && !g_next_track_flag && !g_prev_track_flag && g_is_playing) {
            yasp_usleep(100000); // Sleep 100ms
        }

        uint8_t cmd = s98->buffer[s98->pos++];
        switch (cmd) {
            case 0x00: // YM2151 (OPM)
            case 0x01: // YM2203 (OPN)
            case 0x02: // YM2612 (OPN2)
            case 0x03: // YM2608 (OPNA)
            case 0x04: // YM2413 (OPLL)
            case 0x05: // YM3812 (OPL2)
            case 0x06: // YM3526 (OPL)
            case 0x07: // Y8950 (MSX-AUDIO)
            case 0x08: // YMF262 (OPL3)
            {
                uint8_t addr = s98->buffer[s98->pos++];
                uint8_t data = s98->buffer[s98->pos++];
                // This is a simplified mapping. A more robust player would use the S98v3 device info block.
                if (cmd == 0x00) spfm_write_reg(get_slot_for_chip(CHIP_TYPE_YM2151), 0, addr, data);
                else if (cmd == 0x01) spfm_write_reg(get_slot_for_chip(CHIP_TYPE_YM2203), 0, addr, data);
                else if (cmd == 0x02) spfm_write_reg(get_slot_for_chip(CHIP_TYPE_YM2612), (addr & 0x100) ? 1 : 0, addr & 0xff, data);
                else if (cmd == 0x03) spfm_write_reg(get_slot_for_chip(CHIP_TYPE_YM2608), (addr & 0x100) ? 1 : 0, addr & 0xff, data);
                else if (cmd == 0x04) spfm_write_reg(get_slot_for_chip(CHIP_TYPE_YM2413), 0, addr, data);
                else if (cmd == 0x05) spfm_write_reg(get_slot_for_chip(CHIP_TYPE_YM3812), 0, addr, data);
                else if (cmd == 0x06) spfm_write_reg(get_slot_for_chip(CHIP_TYPE_YM3526), 0, addr, data);
                else if (cmd == 0x07) spfm_write_reg(get_slot_for_chip(CHIP_TYPE_Y8950), 0, addr, data);
                else if (cmd == 0x08) spfm_write_reg(get_slot_for_chip(CHIP_TYPE_YMF262), (addr & 0x100) ? 1 : 0, addr & 0xff, data);
                break;
            }
            case 0x10: // AY-3-8910 (PSG)
            {
                uint8_t addr = s98->buffer[s98->pos++];
                uint8_t data = s98->buffer[s98->pos++];
                spfm_write_reg(get_slot_for_chip(CHIP_TYPE_AY8910), 0, addr, data);
                break;
            }
            case 0x11: // SN76489 (DCSG)
            {
                uint8_t data = s98->buffer[s98->pos++];
                spfm_write_data(get_slot_for_chip(CHIP_TYPE_SN76489), data);
                break;
            }
            case 0xFF: // sync
                spfm_flush();
                sync_count = s98_get_val(s98) + 1;
                // Use 64-bit integers for calculation to prevent overflow
                spfm_wait_and_write_reg(((uint64_t)sync_count * sync_wait * 44100) / 1000000, 0, 0, 0, 0);
                break;
            case 0xFE: // sync (variable)
                spfm_flush();
                sync_count = s98_get_val(s98); // Value is the number of milliseconds
                if (sync_count > 0) {
                    // Wait for 'sync_count' milliseconds
                    spfm_wait_and_write_reg(((uint64_t)sync_count * 1000 * 44100) / 1000000, 0, 0, 0, 0);
                }
                break;
            case 0xFD: // loop
                if (s98->offset_to_loop != 0) {
                    s98->pos = s98->offset_to_loop;
                }
                break;
            case 0xFC: // end
                g_is_playing = false; // End of song
                break;
            default:
                // Unsupported chip, but we continue parsing for syncs
                if (cmd >= 0x10 && cmd <= 0x1F) { // PSG
                    s98->pos += 1;
                } else if (cmd >= 0x80) { // Other devices often have 2 params
                    s98->pos += 2;
                }
                break;
        }
    }
    spfm_flush();
    g_is_playing = false; // Signal that playback for this file is complete
    return true;
}

static uint32_t s98_get_val(S98* s98) {
    uint32_t val = 0;
    int shift = 0;
    uint8_t b;
    while (s98->pos < s98->size) {
        b = s98->buffer[s98->pos++];
        val |= (uint32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) {
            return val;
        }
        shift += 7;
        // Prevent infinite loops and overflows on malformed data
        if (shift >= 32) {
            return val;
        }
    }
    return val; // Reached end of file unexpectedly
}
