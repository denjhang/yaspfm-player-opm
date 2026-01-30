#include "saa1099.h"
#include "saa1099_viz.h"
#include "spfm.h"
#include "chiptype.h"
#include "vgm.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

// External flush mode control (same as YM2151 for timer optimization)
extern volatile int g_flush_mode;

// Channel 0 mapping configuration
static volatile saa1099_ch0_map_mode_t g_saa_ch0_map_mode = SAA_CH0_MAP_NONE;
static volatile saa1099_map_type_t g_map_type = SAA_MAP_TYPE_ALL;
static volatile uint8_t g_map_target_channel = 0;  // 0-5

// Ch0 repeat write count for testing (0 = no repeat, 1+ = additional writes)
static volatile uint8_t g_ch0_repeat_count = 0;

// Ch0 delay samples (0 = no delay, 10-1000 = delay in VGM samples after each ch0 write, step 10)
static volatile uint16_t g_ch0_delay_samples = 0;

// Auto-cycle configuration (based on 0x80 wait command count, not real time)
typedef enum {
    SAA_AUTO_CYCLE_OFF = 0,        // No auto-cycling
    SAA_AUTO_CYCLE_TONE_ALL,       // Tone ↔ All (skip Noise)
    SAA_AUTO_CYCLE_NOISE_ALL,      // Noise ↔ All (skip Tone)
    SAA_AUTO_CYCLE_FULL,           // Tone → Noise → All → Tone (full cycle)
    SAA_AUTO_CYCLE_MODE_COUNT
} saa1099_auto_cycle_mode_t;

static volatile saa1099_auto_cycle_mode_t g_auto_cycle_mode = SAA_AUTO_CYCLE_OFF;
static volatile uint32_t g_auto_cycle_interval = 0;    // 0x80 count interval (0 = disabled)
static volatile uint32_t g_auto_cycle_counter = 0;     // Current 0x80 count

// Prevent recursive mapping
static volatile int g_in_mapping = 0;

void saa1099_write_reg(uint8_t slot, uint8_t addr, uint8_t data) {
    // Save original values for visualization (before mapping)
    uint8_t original_addr = addr;
    uint8_t original_data = data;

    // Channel 0 mapping logic - ONLY apply to Slot0!
    // This prevents conflicts when VGM uses multiple SAA1099 chips
    // Also prevent recursive mapping
    saa1099_ch0_map_mode_t map_mode = g_saa_ch0_map_mode;

    if (slot == 0 && map_mode != SAA_CH0_MAP_NONE && !g_in_mapping) {
        g_in_mapping = 1;  // Set flag to prevent recursion
        uint8_t reg = addr & 0x1F;

        if (map_mode == SAA_CH0_MAP_MUTE) {
            // Mute mode: Simply block all channel 0 output
            if (reg == 0x00) { g_in_mapping = 0; return; }  // Block amplitude
            if (reg == 0x08) { g_in_mapping = 0; return; }  // Block frequency
            if (reg == 0x10) { data &= 0xF0; }  // Block octave (clear ch0 bits)
            if (reg == 0x14) { data &= ~0x01; }  // Block tone enable
            if (reg == 0x15) { data &= ~0x01; }  // Block noise enable
            if (reg == 0x16) { data &= ~0x03; }  // Block noise control
        }
        else if (map_mode == SAA_CH0_MAP_TO_SLOT0 || map_mode == SAA_CH0_MAP_TO_SLOT1) {
            // Map mode: Route ch0 to target channel with configurable mapping
            saa1099_map_type_t map_type = g_map_type;
            uint8_t target_channel = g_map_target_channel;  // 0-5
            uint8_t target_slot = (map_mode == SAA_CH0_MAP_TO_SLOT1) ? 1 : 0;

            // Special case: If mapping Slot0 Ch0 to itself, bypass mapping logic
            // This allows testing if direct writes can restore Ch0 functionality
            if (target_slot == 0 && target_channel == 0) {
                g_in_mapping = 0;
                // Fall through to normal write - don't block or redirect
                goto normal_write;
            }

            // Check if target slot is valid for Slot1
            extern chip_type_t g_slot_to_chip[2];
            if (target_slot == 1 && g_slot_to_chip[1] != CHIP_TYPE_SAA1099) {
                // No second chip, just mute channel 0
                if (reg == 0x00) { g_in_mapping = 0; return; }
                if (reg == 0x08) { g_in_mapping = 0; return; }
                if (reg == 0x10) { data &= 0xF0; }
                if (reg == 0x14) { data &= ~0x01; }
                if (reg == 0x15) { data &= ~0x01; }
                if (reg == 0x16) { data &= ~0x03; }
                g_in_mapping = 0;
                return;
            }

            bool map_tone = (map_type == SAA_MAP_TYPE_TONE || map_type == SAA_MAP_TYPE_ALL);
            bool map_noise = (map_type == SAA_MAP_TYPE_NOISE || map_type == SAA_MAP_TYPE_ALL);

            // Amplitude - always map with tone
            if (reg == 0x00 && map_tone) {
                saa1099_write_reg(target_slot, 0x00 + target_channel, data);
                g_in_mapping = 0;
                return;  // Block ch0 on Slot0
            }

            // Frequency - tone mapping
            if (reg == 0x08 && map_tone) {
                saa1099_write_reg(target_slot, 0x08 + target_channel, data);
                g_in_mapping = 0;
                return;
            }

            // Octave - tone mapping
            if (reg == 0x10 && map_tone) {
                uint8_t ch0_octave = data & 0x0F;
                // Octave registers: 0x10 for ch0-1, 0x11 for ch2-3, 0x12 for ch4-5
                uint8_t octave_reg = 0x10 + (target_channel / 2);
                uint8_t shift = (target_channel % 2) * 4;

                // Read-modify-write for octave register
                // For simplicity, we'll just send the target octave value
                saa1099_write_reg(target_slot, octave_reg, ch0_octave << shift);

                // Block ch0 on Slot0
                data &= 0xF0;
            }

            // Tone enable - tone mapping
            if (reg == 0x14 && map_tone) {
                if (data & 0x01) {
                    saa1099_write_reg(target_slot, 0x14, 1 << target_channel);
                }
                data &= ~0x01;  // Disable ch0 on Slot0
            }

            // Noise enable - noise mapping
            if (reg == 0x15 && map_noise) {
                if (data & 0x01) {
                    saa1099_write_reg(target_slot, 0x15, 1 << target_channel);
                }
                data &= ~0x01;  // Disable noise on Slot0 ch0
            }

            // Noise control - noise mapping
            // Ch0,1,2 use bits 0-2 (noise gen 0), Ch3,4,5 use bits 4-6 (noise gen 1)
            if (reg == 0x16 && map_noise) {
                uint8_t ch0_noise = data & 0x07;  // Get ch0 noise control (bits 0-2)
                uint8_t shift = (target_channel <= 2) ? 0 : 4;
                saa1099_write_reg(target_slot, 0x16, ch0_noise << shift);
                data &= ~0x07;  // Block noise control for ch0 on Slot0
            }

            // Master enable - forward to target slot if needed
            if (reg == 0x1C && target_slot == 1) {
                saa1099_write_reg(target_slot, 0x1C, data);
            }
        }

        g_in_mapping = 0;  // Clear flag after mapping
    }

normal_write:
    // SAA1099 TWO-STEP WRITE PROTOCOL (discovered from libvgm):
    //
    // Reference: libvgm/player/vgmplayer_cmdhandler.cpp:1337-1338
    //   cDev->write8(dataPtr, 0x01, addr);  // offset 1 → select register
    //   cDev->write8(dataPtr, 0x00, data);  // offset 0 → write data
    //
    // SAA1099 requires TWO hardware writes (see saa1099_mame.c:470):
    //   if (offset & 1) saa1099_control_w(addr);  // odd → select register
    //   else            saa1099_data_w(data);     // even → write data
    //
    // Step 1: Select register - send command with "offset 1" (odd address)
    // Step 2: Write data - send command with "offset 0" (even address)
    //
    // In SPFM protocol, we send [slot, port<<1, byte3, byte4]
    // The port parameter (shifted) becomes the "address" that controls A0 signals
    //
    // Step 1: Select register using port=1 (A0=1)
    // Step 2: Write data using port=0 (A0=0)

    // Step 1: Select register - send addr as byte4 (always A0=1)
    spfm_write_reg(slot, 1, 0x00, addr & 0x1F);
    spfm_wait_and_write_reg(0, 0, 0, 0, 0);

    // Step 2: Write data - send data as byte3 (always A0=0)
    spfm_write_reg(slot, 0, data, 0x00);

    // Update visualization state with original values (before mapping)
    saa1099_viz_update_state(slot, original_addr & 0x1F, original_data);

    // Ch0 repeat write testing: Repeat writes to Ch0 registers to test if this can restore functionality
    // Only apply to Slot0 Ch0 registers (amplitude, frequency, octave, enables)
    if (slot == 0 && g_ch0_repeat_count > 0) {
        uint8_t reg = addr & 0x1F;
        bool is_ch0_reg = (reg == 0x00) || (reg == 0x08) || (reg == 0x10) ||
                          (reg == 0x14) || (reg == 0x15) || (reg == 0x16);

        if (is_ch0_reg) {
            for (uint8_t i = 0; i < g_ch0_repeat_count; i++) {
                // Repeat the two-step write protocol
                spfm_write_reg(slot, 1, 0x00, addr & 0x1F);
                spfm_wait_and_write_reg(0, 0, 0, 0, 0);
                spfm_write_reg(slot, 0, data, 0x00);
            }
        }
    }

    // Ch0 delay: Add delay in VGM samples after each ch0 write
    // Only apply to Slot0 Ch0 registers
    if (slot == 0 && g_ch0_delay_samples > 0) {
        uint8_t reg = addr & 0x1F;
        bool is_ch0_reg = (reg == 0x00) || (reg == 0x08) || (reg == 0x10) ||
                          (reg == 0x14) || (reg == 0x15) || (reg == 0x16);

        if (is_ch0_reg) {
            // Add delay using spfm_wait_and_write_reg
            spfm_wait_and_write_reg(g_ch0_delay_samples, 0, 0, 0, 0);
        }
    }

    // Timer optimization: flush after each complete register write (both steps done)
    // This prevents audio glitches when user interacts with other windows
    if (g_flush_mode == 1) { // Register-level flush
        spfm_flush();
    }
}

void saa1099_mute(uint8_t slot) {
    // Master enable off
    saa1099_write_reg(slot, 0x1C, 0x00);

    // Clear all amplitudes
    for (uint8_t i = 0x00; i <= 0x05; i++) {
        saa1099_write_reg(slot, i, 0x00);
    }
}

void saa1099_init(uint8_t slot) {
    // SAA1099 initialization by replicating BackToRe_trimmed.vgm init sequence
    // Reference: BackToRe_trimmed.vgm offsets 0x1C0-0x24C
    // This is the complete MAME-recorded initialization that works correctly

    // Helper macro for two-step write
    // CRITICAL FIX: Use correct two-step protocol matching saa1099_write_reg()
    // Step 1: port=1 (A0=1) to select register
    // Step 2: port=0 (A0=0) to write data
    #define SAA_WRITE(reg, val) do { \
        spfm_write_reg(slot, 1, 0x00, (reg) & 0x1F); \
        spfm_write_reg(slot, 0, (val), 0x00); \
    } while(0)

    // Offset 0x1C0: Enable master control
    SAA_WRITE(0x1C, 0x01);

    // Offset 0x1C3-0x1CF: Clear control registers
    SAA_WRITE(0x14, 0x00);  // Channel enable
    SAA_WRITE(0x15, 0x00);  // Noise enable
    SAA_WRITE(0x16, 0x00);  // Noise control
    SAA_WRITE(0x18, 0x00);  // Envelope 0
    SAA_WRITE(0x19, 0x00);  // Envelope 1
    spfm_flush();

    // Offset 0x1D2: Wait 4267 samples (≈97ms @ 44.1kHz)
#ifdef _WIN32
    Sleep(97);
#else
    usleep(97000);
#endif

    // Offset 0x1D5-0x20C: Clear all amplitude, frequency, octave registers
    SAA_WRITE(0x00, 0x00);  // Amplitude ch0
    SAA_WRITE(0x01, 0x00);  // Amplitude ch1
    SAA_WRITE(0x02, 0x00);  // Amplitude ch2
    SAA_WRITE(0x03, 0x00);  // Amplitude ch3
    SAA_WRITE(0x04, 0x00);  // Amplitude ch4
    SAA_WRITE(0x05, 0x00);  // Amplitude ch5
    SAA_WRITE(0x08, 0x00);  // Frequency ch0
    SAA_WRITE(0x09, 0x00);  // Frequency ch1
    SAA_WRITE(0x0A, 0x00);  // Frequency ch2
    SAA_WRITE(0x0B, 0x00);  // Frequency ch3
    SAA_WRITE(0x0C, 0x00);  // Frequency ch4
    SAA_WRITE(0x0D, 0x00);  // Frequency ch5
    SAA_WRITE(0x10, 0x00);  // Octave 0-1
    SAA_WRITE(0x11, 0x00);  // Octave 2-3
    SAA_WRITE(0x12, 0x00);  // Octave 4-5
    SAA_WRITE(0x14, 0x00);  // Channel enable
    SAA_WRITE(0x15, 0x00);  // Noise enable
    SAA_WRITE(0x16, 0x00);  // Noise control
    spfm_flush();

    // Offset 0x20F: Wait 876 samples (≈20ms)
#ifdef _WIN32
    Sleep(20);
#else
    usleep(20000);
#endif

    // Done - chip is now in clean initialized state
    // VGM files will write their own values from here

    #undef SAA_WRITE
}

// Channel 0 mapping functions
void saa1099_set_ch0_map_mode(saa1099_ch0_map_mode_t mode) {
    if (mode < SAA_CH0_MAP_MODE_COUNT) {
        g_saa_ch0_map_mode = mode;
    }
}

saa1099_ch0_map_mode_t saa1099_get_ch0_map_mode(void) {
    return g_saa_ch0_map_mode;
}

void saa1099_set_map_type(saa1099_map_type_t type) {
    if (type < SAA_MAP_TYPE_COUNT) {
        g_map_type = type;
    }
}

saa1099_map_type_t saa1099_get_map_type(void) {
    return g_map_type;
}

void saa1099_set_map_target_channel(uint8_t channel) {
    if (channel <= 5) {
        g_map_target_channel = channel;
    }
}

uint8_t saa1099_get_map_target_channel(void) {
    return g_map_target_channel;
}

void saa1099_cycle_map_type(void) {
    g_map_type = (g_map_type + 1) % SAA_MAP_TYPE_COUNT;
}

void saa1099_cycle_map_target_channel(void) {
    g_map_target_channel++;
    if (g_map_target_channel > 5) {
        g_map_target_channel = 0;
    }
}

const char* saa1099_get_map_status_string(char* buffer, size_t size) {
    if (g_saa_ch0_map_mode == SAA_CH0_MAP_NONE) {
        snprintf(buffer, size, "None");
    } else if (g_saa_ch0_map_mode == SAA_CH0_MAP_MUTE) {
        snprintf(buffer, size, "Ch0 Mute");
    } else if (g_saa_ch0_map_mode == SAA_CH0_MAP_TO_SLOT0 || g_saa_ch0_map_mode == SAA_CH0_MAP_TO_SLOT1) {
        const char* type_name = "";
        switch (g_map_type) {
            case SAA_MAP_TYPE_TONE: type_name = "Tone"; break;
            case SAA_MAP_TYPE_NOISE: type_name = "Noise"; break;
            case SAA_MAP_TYPE_ALL: type_name = "All"; break;
            default: type_name = "?"; break;
        }

        const char* slot_name = (g_saa_ch0_map_mode == SAA_CH0_MAP_TO_SLOT0) ? "Slot0" : "Slot1";

        snprintf(buffer, size, "%s->%s Ch%d", type_name, slot_name, g_map_target_channel);
    } else {
        snprintf(buffer, size, "Unknown");
    }
    return buffer;
}

void saa1099_set_ch0_repeat_count(uint8_t count) {
    if (count <= 100) {  // Allow up to 100 repeats
        g_ch0_repeat_count = count;
    }
}

uint8_t saa1099_get_ch0_repeat_count(void) {
    return g_ch0_repeat_count;
}

void saa1099_increase_ch0_repeat_count(void) {
    if (g_ch0_repeat_count <= 90) {
        g_ch0_repeat_count += 10;
    } else if (g_ch0_repeat_count < 100) {
        g_ch0_repeat_count = 100;
    }
}

void saa1099_decrease_ch0_repeat_count(void) {
    if (g_ch0_repeat_count >= 10) {
        g_ch0_repeat_count -= 10;
    } else if (g_ch0_repeat_count > 0) {
        g_ch0_repeat_count = 0;
    }
}

// Ch0 delay samples control functions
void saa1099_increase_ch0_delay_samples(void) {
    if (g_ch0_delay_samples < 1000) {
        g_ch0_delay_samples += 10;
    }
}

void saa1099_decrease_ch0_delay_samples(void) {
    if (g_ch0_delay_samples >= 10) {
        g_ch0_delay_samples -= 10;
    }
}

uint16_t saa1099_get_ch0_delay_samples(void) {
    return g_ch0_delay_samples;
}

void saa1099_cycle_map_type_fast(void) {
    // Restore full cycle: Tone -> Noise -> All -> Tone
    saa1099_cycle_map_type();
}

// Auto-cycle interval control functions (in 0x80 command counts)
void saa1099_increase_auto_cycle_interval(void) {
    // Increment by 100 0x80 commands
    if (g_auto_cycle_interval < 10000) {
        g_auto_cycle_interval += 100;
    }
}

void saa1099_decrease_auto_cycle_interval(void) {
    // Decrement by 100 0x80 commands, min 0
    if (g_auto_cycle_interval >= 100) {
        g_auto_cycle_interval -= 100;
    } else {
        g_auto_cycle_interval = 0;
    }
}

uint32_t saa1099_get_auto_cycle_interval(void) {
    return g_auto_cycle_interval;
}

// Called from SPFM layer when 0x80 hardware delay command is used
void saa1099_on_spfm_delay_0x80(void) {
    if (g_auto_cycle_mode == SAA_AUTO_CYCLE_OFF || g_auto_cycle_interval == 0) {
        return;  // Auto-cycle disabled
    }

    g_auto_cycle_counter++;

    // Check if we should cycle
    if (g_auto_cycle_counter >= g_auto_cycle_interval) {
        g_auto_cycle_counter = 0;  // Reset counter

        // Perform the cycle based on mode
        switch (g_auto_cycle_mode) {
            case SAA_AUTO_CYCLE_TONE_ALL:
                // Toggle between Tone and All
                if (g_map_type == SAA_MAP_TYPE_TONE) {
                    g_map_type = SAA_MAP_TYPE_ALL;
                } else {
                    g_map_type = SAA_MAP_TYPE_TONE;
                }
                break;

            case SAA_AUTO_CYCLE_NOISE_ALL:
                // Toggle between Noise and All
                if (g_map_type == SAA_MAP_TYPE_NOISE) {
                    g_map_type = SAA_MAP_TYPE_ALL;
                } else {
                    g_map_type = SAA_MAP_TYPE_NOISE;
                }
                break;

            case SAA_AUTO_CYCLE_FULL:
                // Full cycle: Tone -> Noise -> All -> Tone
                saa1099_cycle_map_type();
                break;

            default:
                break;
        }
    }
}

// Cycle through auto-cycle modes
void saa1099_cycle_auto_cycle_mode(void) {
    g_auto_cycle_mode = (saa1099_auto_cycle_mode_t)((g_auto_cycle_mode + 1) % SAA_AUTO_CYCLE_MODE_COUNT);
    g_auto_cycle_counter = 0;  // Reset counter when changing mode

    // Set initial map type based on mode
    if (g_auto_cycle_mode != SAA_AUTO_CYCLE_OFF) {
        switch (g_auto_cycle_mode) {
            case SAA_AUTO_CYCLE_TONE_ALL:
                g_map_type = SAA_MAP_TYPE_TONE;
                break;
            case SAA_AUTO_CYCLE_NOISE_ALL:
                g_map_type = SAA_MAP_TYPE_NOISE;
                break;
            case SAA_AUTO_CYCLE_FULL:
                g_map_type = SAA_MAP_TYPE_TONE;
                break;
            default:
                break;
        }
    }
}

// Get auto-cycle mode name
const char* saa1099_get_auto_cycle_mode_name(void) {
    switch (g_auto_cycle_mode) {
        case SAA_AUTO_CYCLE_OFF: return "Off";
        case SAA_AUTO_CYCLE_TONE_ALL: return "Tone<->All";
        case SAA_AUTO_CYCLE_NOISE_ALL: return "Noise<->All";
        case SAA_AUTO_CYCLE_FULL: return "Full";
        default: return "?";
    }
}
