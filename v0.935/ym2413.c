#include "ym2413.h"
#include "spfm.h"

// YM2413 a.k.a OPLL

void ym2413_write_reg(uint8_t slot, uint8_t addr, uint8_t data) {
    // YM2413 only has one port.
    spfm_write_reg(slot, 0, addr, data);
}

// Silences the YM2413 chip.
void ym2413_mute(uint8_t slot) {
    int i;
    // Key off all 9 channels
    for (i = 0; i < 9; i++) {
        ym2413_write_reg(slot, 0x20 + i, 0x00);
    }
    // Set volume to max attenuation for all channels
    for (i = 0; i < 9; i++) {
        ym2413_write_reg(slot, 0x30 + i, 0x0F);
    }
    // Disable rhythm mode
    ym2413_write_reg(slot, 0x0E, 0x00);
}

// Initializes the YM2413 to a clean state.
void ym2413_init(uint8_t slot) {
    int i;

    // Mute the chip completely.
    ym2413_mute(slot);

    // Zero out all other registers
    for (i = 0; i <= 0x07; i++) {
        ym2413_write_reg(slot, (uint8_t)i, 0x00);
    }
    for (i = 0x10; i <= 0x18; i++) {
        ym2413_write_reg(slot, (uint8_t)i, 0x00);
    }
    for (i = 0x20; i <= 0x28; i++) {
        ym2413_write_reg(slot, (uint8_t)i, 0x00);
    }
    
    spfm_flush();
}
