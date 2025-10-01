#include "y8950.h"
#include "spfm.h"
#include "chiptype.h"

// Y8950 a.k.a. MSX-Audio

void y8950_write_reg(uint8_t slot, uint8_t reg, uint8_t data) {
    // Y8950 is similar to other OPL chips, write address then data.
    spfm_write_reg(slot, 0, reg, data);
}

// Silences the Y8950 chip.
void y8950_mute(uint8_t slot) {
    int i;
    // Key off all 9 FM channels
    for (i = 0; i < 9; i++) {
        y8950_write_reg(slot, 0xB0 + i, 0x00);
    }
    // Set volume to max attenuation for all 9 FM channels
    for (i = 0; i < 9; i++) {
        y8950_write_reg(slot, 0x40 + i, 0x3F);
    }
    // Mute rhythm channels
    y8950_write_reg(slot, 0xBD, 0x00);
    // Mute ADPCM channel
    y8950_write_reg(slot, 0x07, 0x80);
}

// Initializes the Y8950 to a clean state.
void y8950_init(uint8_t slot) {
    int i;
    // Mute all channels
    y8950_mute(slot);

    // Zero out all other registers
    for (i = 0x00; i <= 0x18; i++) {
        y8950_write_reg(slot, (uint8_t)i, 0x00);
    }
    for (i = 0x20; i <= 0xF5; i++) {
        y8950_write_reg(slot, (uint8_t)i, 0x00);
    }

    spfm_flush();
}
