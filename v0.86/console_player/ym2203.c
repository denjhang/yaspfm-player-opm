#include "ym2203.h"
#include "spfm.h"

// YM2203 a.k.a OPN

void ym2203_write_reg(uint8_t slot, uint8_t addr, uint8_t data) {
    // YM2203 only has one port.
    spfm_write_reg(slot, 0, addr, data);
}

// Silences the YM2203 chip.
void ym2203_mute(uint8_t slot) {
    int i;
    // Key off all 3 FM channels
    for (i = 0; i < 3; i++) {
        ym2203_write_reg(slot, 0x28, (uint8_t)i);
    }
    // Mute SSG channels
    ym2203_write_reg(slot, 0x08, 0x00);
    ym2203_write_reg(slot, 0x09, 0x00);
    ym2203_write_reg(slot, 0x0A, 0x00);
    // Set SSG mixer to disable tone and noise
    ym2203_write_reg(slot, 0x07, 0x3F);
}

// Initializes the YM2203 to a clean state.
void ym2203_init(uint8_t slot) {
    int i, j;

    // Mute the chip completely.
    ym2203_mute(slot);

    // Reset LFO
    ym2203_write_reg(slot, 0x22, 0x00);

    // Turn off timers
    ym2203_write_reg(slot, 0x27, 0x00);

    // Zero out all FM channel registers
    for (i = 0; i < 3; i++) {
        // F-Number, Block
        ym2203_write_reg(slot, 0xA4 + i, 0x00);
        ym2203_write_reg(slot, 0xA0 + i, 0x00);
        // Feedback, Algorithm
        ym2203_write_reg(slot, 0xB0 + i, 0x00);
        // Stereo, L/R
        ym2203_write_reg(slot, 0xB4 + i, 0xC0);

        // Zero out operator registers for the channel
        for (j = 0; j < 4; j++) {
            uint8_t op_offset = j * 4;
            // DT, MULT
            ym2203_write_reg(slot, 0x30 + op_offset + i, 0x00);
            // TL
            ym2203_write_reg(slot, 0x40 + op_offset + i, 0x7F);
            // KS, AR
            ym2203_write_reg(slot, 0x50 + op_offset + i, 0x1F);
            // AM, D1R
            ym2203_write_reg(slot, 0x60 + op_offset + i, 0x00);
            // D2R
            ym2203_write_reg(slot, 0x70 + op_offset + i, 0x00);
            // D1L, RR
            ym2203_write_reg(slot, 0x80 + op_offset + i, 0x0F);
        }
    }

    // Zero out SSG registers
    for (i = 0; i < 14; i++) {
        ym2203_write_reg(slot, (uint8_t)i, 0x00);
    }
    
    spfm_flush();
}
