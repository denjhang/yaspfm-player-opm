#include "ymf262.h"
#include "spfm.h"
#include "chiptype.h"

// YMF262 (OPL3)

void ymf262_write_reg(uint8_t slot, uint8_t port, uint8_t reg, uint8_t data) {
    // YMF262 is an OPL series chip, write address then data.
    // It has two register sets, selected by the port.
    spfm_write_reg(slot, port, reg, data);
}

// Silences the YMF262 chip.
void ymf262_mute(uint8_t slot) {
    int i;
    // Mute all 18 FM channels (9 per operator block)
    for (i = 0; i < 9; i++) {
        ymf262_write_reg(slot, 0, 0xB0 + i, 0x00); // Primary block
        ymf262_write_reg(slot, 1, 0xB0 + i, 0x00); // Secondary block
    }
    // Set volume to max attenuation for all 18 FM channels
    for (i = 0; i < 9; i++) {
        ymf262_write_reg(slot, 0, 0x40 + i, 0x3F); // Primary block
        ymf262_write_reg(slot, 1, 0x40 + i, 0x3F); // Secondary block
    }
    // Mute rhythm channels
    ymf262_write_reg(slot, 0, 0xBD, 0x00);
}

// Initializes the YMF262 to a clean state.
void ymf262_init(uint8_t slot) {
    int i;
    // Mute all channels
    ymf262_mute(slot);

    // Zero out all other registers for both ports
    for (i = 0x01; i <= 0xF5; i++) {
        // Skip reserved/unused registers
        if (i == 0x06 || i == 0x07 || (i >= 0x09 && i <= 0x1F)) {
            // Skip
        } else {
            ymf262_write_reg(slot, 0, (uint8_t)i, 0x00);
            ymf262_write_reg(slot, 1, (uint8_t)i, 0x00);
        }
    }
    
    // Enable OPL3 mode
    ymf262_write_reg(slot, 1, 0x05, 0x01);
    // Enable Waveform Select
    ymf262_write_reg(slot, 0, 0x01, 0x20);


    spfm_flush();
}
