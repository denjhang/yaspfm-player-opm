#include "ym2608.h"
#include "spfm.h"

// YM2608 a.k.a OPNB

void ym2608_write_reg(uint8_t slot, uint8_t port, uint8_t addr, uint8_t data) {
    spfm_write_reg(slot, port, addr, data);
}

void ym2608_init(uint8_t slot) {
    // This full initialization sequence is based on the one from udpl.
    // It ensures the chip is in a clean state.
    ym2608_write_reg(slot, 0, 0x29, 0x80); // 6ch mode
    ym2608_mute(slot); // Mute all channels
    ym2608_write_reg(slot, 0, 0x22, 0x00); // FM common
    ym2608_write_reg(slot, 0, 0x24, 0x00);
    ym2608_write_reg(slot, 0, 0x25, 0x00);
    ym2608_write_reg(slot, 0, 0x26, 0x00);
    ym2608_write_reg(slot, 0, 0x27, 0x30);
    ym2608_write_reg(slot, 0, 0x29, 0x00); // 3ch mode
}

// Mute the YM2608 chip by turning off all channels and volumes.
// This sequence is based on the initialization from udpl.
void ym2608_mute(uint8_t slot) {
    // FM key off for all 6 channels
    ym2608_write_reg(slot, 0, 0x28, 0x00);
    ym2608_write_reg(slot, 0, 0x28, 0x01);
    ym2608_write_reg(slot, 0, 0x28, 0x02);
    ym2608_write_reg(slot, 0, 0x28, 0x04);
    ym2608_write_reg(slot, 0, 0x28, 0x05);
    ym2608_write_reg(slot, 0, 0x28, 0x06);

    // SSG mute
    ym2608_write_reg(slot, 0, 0x08, 0x00);
    ym2608_write_reg(slot, 0, 0x09, 0x00);
    ym2608_write_reg(slot, 0, 0x0a, 0x00);

    // Rhythm mute
    ym2608_write_reg(slot, 0, 0x11, 0x3f); // rhythm vol 0
    ym2608_write_reg(slot, 0, 0x10, 0xbf); // rhythm mute

    // Individual rhythm volumes to 0
    ym2608_write_reg(slot, 0, 0x18, 0x1f); // BD
    ym2608_write_reg(slot, 0, 0x19, 0x1f); // SD
    ym2608_write_reg(slot, 0, 0x1a, 0x1f); // TOP
    ym2608_write_reg(slot, 0, 0x1b, 0x1f); // HH
    ym2608_write_reg(slot, 0, 0x1c, 0x1f); // TOM
    ym2608_write_reg(slot, 0, 0x1d, 0x1f); // RYM

    // ADPCM mute
    ym2608_write_reg(slot, 1, 0x0b, 0x00);
}
