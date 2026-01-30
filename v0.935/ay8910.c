#include "ay8910.h"
#include "spfm.h"
#include "chiptype.h"

// AY8910 a.k.a. PSG

void ay8910_write_reg(uint8_t slot, uint8_t reg, uint8_t data) {
    // SPFM protocol: single 4-byte command [slot, port<<1, register, data]
    // The addr parameter IS the register number, NOT an A0 control signal!
    // Verified working with test_ay8910_combined.exe
    spfm_write_reg(slot, 0, reg, data);
}

// Silences the AY8910 chip.
void ay8910_mute(uint8_t slot) {
    int i;
    // Disable tone and noise for all 3 channels
    ay8910_write_reg(slot, 0x07, 0b00111111);

    // Set volume to 0 for all 3 channels
    for (i = 0; i < 3; i++) {
        ay8910_write_reg(slot, 0x08 + i, 0x00);
    }
}

// Initializes the AY8910 to a clean state.
void ay8910_init(uint8_t slot) {
    int i;
    // Mute all channels
    ay8910_mute(slot);

    // Zero out all registers
    for (i = 0; i < 14; i++) {
        ay8910_write_reg(slot, (uint8_t)i, 0x00);
    }
    
    spfm_flush();
}
