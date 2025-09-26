#include "ym3812.h"
#include "spfm.h"
#include "chiptype.h"

// YM3812 (OPL2)

void ym3812_write_reg(uint8_t slot, uint8_t reg, uint8_t data) {
    // YM3812 is an OPL series chip, write address then data.
    spfm_write_reg(slot, 0, reg, data);
}

// Silences the YM3812 chip.
void ym3812_mute(uint8_t slot) {
    int i;
    // Key off all 9 FM channels
    for (i = 0; i < 9; i++) {
        ym3812_write_reg(slot, 0xB0 + i, 0x00);
    }
    // Set volume to max attenuation for all 9 FM channels
    for (i = 0; i < 9; i++) {
        ym3812_write_reg(slot, 0x40 + i, 0x3F);
    }
    // Mute rhythm channels
    ym3812_write_reg(slot, 0xBD, 0x00);
}

// Initializes the YM3812 to a clean state.
void ym3812_init(uint8_t slot) {
    int i;
    // Mute all channels
    ym3812_mute(slot);

    // Zero out all other registers
    for (i = 0x01; i <= 0xF5; i++) {
        // Skip reserved/unused registers. YM3812 has fewer reserved
        // registers than YM3526, but it's still good practice.
        if (i == 0x06 || i == 0x07 || (i >= 0x09 && i <= 0x1F)) {
            // Skip these ranges
        } else {
            ym3812_write_reg(slot, (uint8_t)i, 0x00);
        }
    }
    
    // Enable Waveform Select
    ym3812_write_reg(slot, 0x01, 0x20);

    spfm_flush();
}
