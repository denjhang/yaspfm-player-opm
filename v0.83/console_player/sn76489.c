#include "sn76489.h"
#include "spfm.h"
#include "chiptype.h"

// SN76489 a.k.a. PSG

void sn76489_write_reg(uint8_t slot, uint8_t data) {
    // For SN76489, the address is implicit in the data stream.
    // We use the simplified data-only write command.
    spfm_write_data(slot, data);
}

// Silences the SN76489 chip.
void sn76489_mute(uint8_t slot) {
    // Set volume to max attenuation for all 4 channels
    sn76489_write_reg(slot, 0x9F); // Channel 1
    sn76489_write_reg(slot, 0xBF); // Channel 2
    sn76489_write_reg(slot, 0xDF); // Channel 3
    sn76489_write_reg(slot, 0xFF); // Channel 4
}

// Initializes the SN76489 to a clean state.
void sn76489_init(uint8_t slot) {
    // Mute all channels
    sn76489_mute(slot);

    // Set frequency to 0 for all 3 tone channels
    sn76489_write_reg(slot, 0x80); // Channel 1
    sn76489_write_reg(slot, 0x00);
    sn76489_write_reg(slot, 0xA0); // Channel 2
    sn76489_write_reg(slot, 0x00);
    sn76489_write_reg(slot, 0xC0); // Channel 3
    sn76489_write_reg(slot, 0x00);

    // Set noise channel to white noise, max frequency
    sn76489_write_reg(slot, 0xE0);

    spfm_flush();
}
