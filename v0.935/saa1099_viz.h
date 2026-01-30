#ifndef _SAA1099_VIZ_H_
#define _SAA1099_VIZ_H_

#include <stdint.h>
#include <stdbool.h>

// SAA1099 register state structure
typedef struct {
    uint8_t amplitude[6];      // 0x00-0x05: Amplitude (bit7-4=Right, bit3-0=Left)
    uint8_t frequency[6];      // 0x08-0x0D: Frequency values
    uint8_t octave[3];         // 0x10-0x12: Octave control (2 channels per reg)
    uint8_t channel_enable;    // 0x14: Channel enable bits
    uint8_t noise_enable;      // 0x15: Noise enable bits
    uint8_t noise_control;     // 0x16: Noise generator control
    uint8_t envelope[2];       // 0x18-0x19: Envelope generators
    uint8_t last_reg;          // Last register written
    uint8_t last_data;         // Last data written
    bool enabled;              // Whether this slot is active
} saa1099_state_t;

#ifdef __cplusplus
extern "C" {
#endif

// Initialize visualization system
void saa1099_viz_init(void);

// Cleanup visualization system (call on program exit)
void saa1099_viz_cleanup(void);

// Start visualization window (in separate thread)
void saa1099_viz_start(void);

// Stop visualization window
void saa1099_viz_stop(void);

// Update state for a specific slot (called from saa1099_write_reg)
void saa1099_viz_update_state(uint8_t slot, uint8_t reg, uint8_t data);

// Enable/disable visualization for a slot
void saa1099_viz_set_slot_enabled(uint8_t slot, bool enabled);

// Check if a slot's window is currently visible
bool saa1099_viz_is_slot_visible(uint8_t slot);

#ifdef __cplusplus
}
#endif

#endif // _SAA1099_VIZ_H_
