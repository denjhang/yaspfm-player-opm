#include <stdint.h>
#include "ym2151.h"
#include "spfm.h"
#include "chiptype.h"
#include "vgm.h"      // For g_vgm_header
#include "util.h" // For yasp_uspin
#include <math.h> // For log2

// YM2151 a.k.a OPM
extern volatile int g_flush_mode;
extern vgm_header_t g_vgm_header;
extern bool g_opn_to_opm_conversion_enabled;

static uint8_t g_ym2151_regs[256];
static double g_ym2151_clock_ratio = 1.0;
static int g_ym2151_key_diff = 0;
static int g_ym2151_lfo_diff = 0;

// Lookup tables from vgm-conv-main
static const int keyCodeToIndex[] = {0, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 9, 9, 10, 11, 12};
static const int keyIndexToCode[] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14};

void ym2151_write_reg(uint8_t slot, uint8_t addr, uint8_t data) {
    g_ym2151_regs[addr] = data;

    if (g_ym2151_clock_ratio != 1.0 && !g_opn_to_opm_conversion_enabled) {
        if ((0x28 <= addr && addr <= 0x2f) || (0x30 <= addr && addr <= 0x37)) {
            const int ch = addr - (addr < 0x30 ? 0x28 : 0x30);
            const int orgKeyIndex = keyCodeToIndex[g_ym2151_regs[0x28 + ch] & 0xf];
            const int orgKey = (orgKeyIndex << 8) | (g_ym2151_regs[0x30 + ch] & 0xfc);
            int octave = (g_ym2151_regs[0x28 + ch] >> 4) & 0x7;
            int newKey = orgKey + g_ym2151_key_diff;

            if (newKey < 0) {
                if (octave > 0) {
                    octave--;
                    newKey += 12 << 8;
                } else {
                    newKey = 0;
                }
            } else if (newKey >= 12 << 8) {
                if (octave < 7) {
                    octave++;
                    newKey -= 12 << 8;
                } else {
                    newKey = (12 << 8) - 1;
                }
            }
            const uint8_t okc = (octave << 4) | keyIndexToCode[newKey >> 8];
            spfm_write_reg(slot, 0, 0x28 + ch, okc);
            const uint8_t kf = newKey & 0xfc;
            spfm_write_reg(slot, 0, 0x30 + ch, kf);
            if (g_flush_mode == 1) spfm_flush();
            return; // We've sent modified commands
        } else if (addr == 0x0f) { // Noise frequency
            const int nfrq = fmin(0x1f, round((data & 0x1f) * g_ym2151_clock_ratio));
            data = (data & 0xe0) | nfrq;
        } else if (addr == 0x18) { // LFO frequency
            const int lfrq = fmax(0, fmin(255, round(g_ym2151_lfo_diff + data)));
            data = lfrq;
        }
    }

    spfm_write_reg(slot, 0, addr, data);
    if (g_flush_mode == 1) { // Register-level flush
        spfm_flush();
    }
}

void ym2151_mute(uint8_t slot) {
    int i;
    // A buffer large enough for all mute commands.
    // (0xFF-0xE0+1) for SL/RR + 8 for Key-Off + 256 for zeroing = 32 + 8 + 256 = 296
    spfm_reg_t regs[300];
    int count = 0;

    // 1. Set SL=15, RR=15 for all operators
    for (i = 0xE0; i <= 0xFF; i++) {
        regs[count++] = (spfm_reg_t){0, (uint8_t)i, 0xFF};
    }

    // 2. Key off all channels
    for (i = 0; i < 8; i++) {
        regs[count++] = (spfm_reg_t){0, 0x08, (uint8_t)i};
    }

    // 3. Zero all registers for a full reset
    for (i = 0; i <= 0xFF; i++) {
        regs[count++] = (spfm_reg_t){0, (uint8_t)i, 0x00};
    }

    // Write all commands in a single batch
    spfm_write_regs(slot, regs, count, 0);
}

void ym2151_init(uint8_t slot) {
    // Reset clock conversion state
    g_ym2151_clock_ratio = 1.0;
    g_ym2151_key_diff = 0;
    g_ym2151_lfo_diff = 0;

    if (!g_opn_to_opm_conversion_enabled) {
        uint32_t vgm_clock = g_vgm_header.ym2151_clock;
        uint32_t hardware_clock = get_chip_default_clock(CHIP_TYPE_YM2151);
        if (vgm_clock > 0 && vgm_clock != hardware_clock) {
            g_ym2151_clock_ratio = (double)hardware_clock / vgm_clock;
            g_ym2151_key_diff = round(12 * log2(1.0 / g_ym2151_clock_ratio) * 256);
            g_ym2151_lfo_diff = round(16 * log2(1.0 / g_ym2151_clock_ratio));
            logging(INFO, "YM2151 clock mismatch. VGM: %dHz, HW: %dHz. Applying conversion.\n", vgm_clock, hardware_clock);
        }
    }
    
    // Full reset
    for (int i = 0; i <= 0xFF; i++) {
        g_ym2151_regs[i] = 0;
        // Don't use converted write here, just reset the chip state
        spfm_write_reg(slot, 0, (uint8_t)i, 0x00);
    }
    
    spfm_write_reg(slot, 0, 0x01, 0x02); // Test register
    
    // Key off all channels
    for (int i = 0; i < 8; i++) {
        spfm_write_reg(slot, 0, 0x08, (uint8_t)i);
    }

    // Set TL to max for all operators
    for (int i = 0x60; i <= 0x7F; i++) {
        spfm_write_reg(slot, 0, (uint8_t)i, 0x7F);
    }

    // Apply initial LFO diff if needed
    if (g_ym2151_clock_ratio != 1.0 && !g_opn_to_opm_conversion_enabled) {
        const int lfrq = fmax(0, fmin(255, round(g_ym2151_lfo_diff)));
        spfm_write_reg(slot, 0, 0x18, lfrq);
    }
    
    spfm_flush();
}
