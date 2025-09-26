#include "ym3526.h"
#include "spfm.h"
#include "chiptype.h"

// YM3526 (OPL)

void ym3526_write_reg(uint8_t slot, uint8_t reg, uint8_t data) {
    // YM3526 is an OPL series chip, write address then data.
    spfm_write_reg(slot, 0, reg, data);
}

// Silences the YM3526 chip.
void ym3526_mute(uint8_t slot) {
    int i;
    // Key off all 9 FM channels
    for (i = 0; i < 9; i++) {
        ym3526_write_reg(slot, 0xB0 + i, 0x00);
    }
    // Set volume to max attenuation for all 9 FM channels
    for (i = 0; i < 9; i++) {
        ym3526_write_reg(slot, 0x40 + i, 0x3F);
    }
    // Mute rhythm channels
    ym3526_write_reg(slot, 0xBD, 0x00);
}

// Initializes the YM3526 to a clean state.
void ym3526_init(uint8_t slot) {
    int i;
    // Mute all channels
    ym3526_mute(slot);

    // Zero out all other registers
    for (i = 0x01; i <= 0xF5; i++) {
        // Skip reserved/unused registers
        if (i == 0x06 || i == 0x07 || (i >= 0x09 && i <= 0x1F) || (i >= 0x20 && i <= 0x3F) || (i >= 0x40 && i <= 0x5F) || (i >= 0x60 && i <= 0x7F) || (i >= 0x80 && i <= 0x9F) || (i >= 0xA0 && i <= 0xBF) || (i >= 0xC0 && i <= 0xDF) || (i >= 0xE0 && i <= 0xFF)) {
             // Writing to these registers on a real YM3526 can have unpredictable results.
             // It's safer to just initialize the documented ones.
        } else {
            ym3526_write_reg(slot, (uint8_t)i, 0x00);
        }
    }
    
    // Enable Waveform Select
    ym3526_write_reg(slot, 0x01, 0x20);

    spfm_flush();
}
