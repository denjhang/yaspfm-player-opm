#include "saa1099.h"
#include "spfm.h"
#include "chiptype.h"
#include "vgm.h"
#include "util.h"
#include <stdio.h>

void saa1099_write_reg(uint8_t slot, uint8_t addr, uint8_t data) {
    // Method 1: Single command (like AY8910)
    // This produced the strongest response in testing
    spfm_write_reg(slot, 0, addr & 0x1F, data);
}

void saa1099_mute(uint8_t slot) {
    // Master enable off
    saa1099_write_reg(slot, 0x1C, 0x00);

    // Clear all amplitudes
    for (uint8_t i = 0x00; i <= 0x05; i++) {
        saa1099_write_reg(slot, i, 0x00);
    }
}

void saa1099_init(uint8_t slot) {
    // SAA1099 initialization (simplified, based on MegaGrrl)
    // MegaGrrl only writes 0x1C = 0x02 then relies on hardware reset

    // Initialize all 32 registers to 0 (like MSX VGMPlay)
    for (uint8_t i = 0; i < 32; i++) {
        saa1099_write_reg(slot, i, 0x00);
    }

    // Enable sound output via register 0x1C
    // Bit 0 = 1: Enable sound
    saa1099_write_reg(slot, 0x1C, 0x01);

    spfm_flush();
}
