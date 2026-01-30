#include "sn76489.h"
#include "spfm.h"
#include "chiptype.h"
#include "util.h"
#include <windows.h>

// SN76489 a.k.a. PSG

extern volatile int g_flush_mode;


void sn76489_write_reg(uint8_t slot, uint8_t data) {
    // For SN76489, the address is implicit in the data stream.
    // We use the simplified data-only write command.

    // SPFM Light hardware handles bit ordering internally
    // No bit reversal needed - confirmed by hardware testing
    spfm_write_data(slot, data);

    // CRITICAL: Precise delay based on chip clock (like udpl)
    // udpl uses: 32 clocks @ 3.579MHz = (32*1000000/3579545) ≈ 9us
    // Megagrrl uses: 36 clocks @ 3.579MHz ≈ 10us
    // Use 10us software delay for maximum compatibility
    spfm_flush();
    yasp_usleep(10);  // 10 microseconds - matches udpl timing

    // Register-level flush for responsive playback (similar to SAA1099)
    if (g_flush_mode == 1) {
        spfm_flush();
    }

    // NOTE: VGM files have their own timing via wait commands
    // This delay is only for chip write cycle completion
}
void sn76489_write_reg1(uint8_t slot, uint8_t data) {
    // For SN76489, the address is implicit in the data stream.
    // We use the simplified data-only write command.

    // SPFM Light hardware handles bit ordering internally
    // No bit reversal needed - confirmed by hardware testing
    spfm_write_data(slot, data);
    Sleep(20); 
    // Register-level flush for responsive playback (similar to SAA1099)
    if (g_flush_mode == 1) {
        spfm_flush();
    }

    // NOTE: No delay here - VGM files have their own timing via wait commands
    // Delay is only needed during initialization (see sn76489_init)
}


// Silences the SN76489 chip.
void sn76489_mute(uint8_t slot) {
    // Set volume to max attenuation for all 4 channels
    sn76489_write_reg1(slot, 0x9F); // Tone 0 volume = 0xF (silent)
    Sleep(30); // Delay between commands
    sn76489_write_reg1(slot, 0xBF); // Tone 1 volume = 0xF (silent)
    Sleep(30);
    sn76489_write_reg1(slot, 0xDF); // Tone 2 volume = 0xF (silent)
    Sleep(30);
    sn76489_write_reg1(slot, 0xFF); // Noise volume = 0xF (silent)
    Sleep(30);
}

// Initializes the SN76489 to a clean state.
void sn76489_init(uint8_t slot) {
    // CRITICAL ORDER: Mute FIRST, then configure frequencies
    // This prevents startup noise and glitches
    // CRITICAL: Need 30ms delay between commands during initialization
    // Testing shows 20-50ms required for reliable startup

    // Step 1: Mute all 4 channels immediately
    sn76489_write_reg1(slot, 0x9F);  // Tone 0 volume = 0xF (silent)
    Sleep(30);
    sn76489_write_reg1(slot, 0xBF);  // Tone 1 volume = 0xF (silent)
    Sleep(30);
    sn76489_write_reg1(slot, 0xDF);  // Tone 2 volume = 0xF (silent)
    Sleep(30);
    sn76489_write_reg1(slot, 0xFF);  // Noise volume = 0xF (silent)
    Sleep(30);

    // Step 2: Set noise control to safe state (periodic, low frequency)
    sn76489_write_reg1(slot, 0xE0);  // Noise control: periodic, shift rate 512
    Sleep(30);

    // Step 3: Set all tone frequencies to 0
    for (int i = 0; i < 3; i++) {
        sn76489_write_reg1(slot, 0x80 | (i << 5));  // Latch tone freq, low 4 bits = 0
        Sleep(30);
        sn76489_write_reg1(slot, 0x00);              // High 6 bits = 0
        Sleep(30);
    }

    spfm_flush();
}
