#include "ym2612.h"
#include "spfm.h"

// YM2612 a.k.a OPN2

void ym2612_write_reg(uint8_t slot, uint8_t port, uint8_t addr, uint8_t data) {
    spfm_write_reg(slot, port, addr, data);
}

// Silences the YM2612 chip.
void ym2612_mute(uint8_t slot) {
    int i;
    // Key off all 6 channels
    for (i = 0; i < 3; i++) {
        ym2612_write_reg(slot, 0, 0x28, (uint8_t)i);
        ym2612_write_reg(slot, 0, 0x28, (uint8_t)(i + 4));
    }
    // Set release rate to max and disable SSG-EG for all operators
    for (i = 0; i < 6; i++) {
        uint8_t ch_offset = (i < 3) ? i : (i + 1);
        // Set RR to 15 and disable SSG-EG for all 4 operators in the channel
        ym2612_write_reg(slot, 0, 0x80 + ch_offset, 0x0F);
        ym2612_write_reg(slot, 0, 0x84 + ch_offset, 0x0F);
        ym2612_write_reg(slot, 0, 0x88 + ch_offset, 0x0F);
        ym2612_write_reg(slot, 0, 0x8C + ch_offset, 0x0F);
    }
    // Set DAC output to 0
    ym2612_write_reg(slot, 0, 0x2B, 0x00);
}

// Initializes the YM2612 to a clean state.
void ym2612_init(uint8_t slot) {
    int i, j;

    // Mute all channels and reset registers
    ym2612_mute(slot);

    // Reset LFO
    ym2612_write_reg(slot, 0, 0x22, 0x00);

    // Turn off timers
    ym2612_write_reg(slot, 0, 0x27, 0x00);

    // Zero out all channel registers
    for (i = 0; i < 6; i++) {
        uint8_t ch_offset = (i < 3) ? i : (i + 1);
        // F-Number, Block
        ym2612_write_reg(slot, 0, 0xA4 + ch_offset, 0x00);
        ym2612_write_reg(slot, 0, 0xA0 + ch_offset, 0x00);
        // Feedback, Algorithm
        ym2612_write_reg(slot, 0, 0xB0 + ch_offset, 0x00);
        // Stereo, L/R
        ym2612_write_reg(slot, 0, 0xB4 + ch_offset, 0xC0);

        // Zero out operator registers for the channel
        for (j = 0; j < 4; j++) {
            uint8_t op_offset = j * 4;
            // DT, MULT
            ym2612_write_reg(slot, 0, 0x30 + op_offset + ch_offset, 0x00);
            // TL
            ym2612_write_reg(slot, 0, 0x40 + op_offset + ch_offset, 0x7F);
            // KS, AR
            ym2612_write_reg(slot, 0, 0x50 + op_offset + ch_offset, 0x1F);
            // AM, D1R
            ym2612_write_reg(slot, 0, 0x60 + op_offset + ch_offset, 0x00);
            // D2R
            ym2612_write_reg(slot, 0, 0x70 + op_offset + ch_offset, 0x00);
            // D1L, RR
            ym2612_write_reg(slot, 0, 0x80 + op_offset + ch_offset, 0x0F);
        }
    }
    
    // Part 2 registers
    for (i = 0x30; i <= 0x9E; i++) {
        ym2612_write_reg(slot, 1, (uint8_t)i, 0x00);
    }
    for (i = 0xA0; i <= 0xB6; i++) {
        ym2612_write_reg(slot, 1, (uint8_t)i, 0x00);
    }

    spfm_flush();
}
