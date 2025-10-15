#include "ay8910.h"
#include "spfm.h"
#include "chiptype.h"

// AY8910 a.k.a. PSG

void ay8910_write_reg(uint8_t slot, uint8_t reg, uint8_t data) {
    // For AY8910, we first write the register address, then the data.
    // This is done with two separate spfm_write_reg calls.
    // The SPFM board handles the appropriate bus control signals.
    spfm_write_reg(slot, 0, 0, reg);
    spfm_write_reg(slot, 0, 1, data);
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
