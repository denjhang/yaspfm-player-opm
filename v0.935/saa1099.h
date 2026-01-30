#ifndef _SAA1099_H_
#define _SAA1099_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Channel 0 mapping base modes
typedef enum {
    SAA_CH0_MAP_NONE = 0,     // No mapping (default)
    SAA_CH0_MAP_MUTE,         // Mute channel 0 (block all ch0 output)
    SAA_CH0_MAP_TO_SLOT0,     // Map to Slot0 (configurable channel/type)
    SAA_CH0_MAP_TO_SLOT1,     // Map to Slot1 (configurable channel/type)
    SAA_CH0_MAP_MODE_COUNT
} saa1099_ch0_map_mode_t;

// Mapping types (what to map)
typedef enum {
    SAA_MAP_TYPE_TONE = 0,   // Map only tone (frequency, octave, tone enable)
    SAA_MAP_TYPE_NOISE,      // Map only noise (noise enable, noise control)
    SAA_MAP_TYPE_ALL,        // Map both tone and noise
    SAA_MAP_TYPE_COUNT
} saa1099_map_type_t;

void saa1099_init(uint8_t slot);
void saa1099_write_reg(uint8_t slot, uint8_t addr, uint8_t data);
void saa1099_mute(uint8_t slot);

// Channel 0 mapping functions
void saa1099_set_ch0_map_mode(saa1099_ch0_map_mode_t mode);
saa1099_ch0_map_mode_t saa1099_get_ch0_map_mode(void);
void saa1099_set_map_type(saa1099_map_type_t type);
saa1099_map_type_t saa1099_get_map_type(void);
void saa1099_set_map_target_channel(uint8_t channel);  // 0-5
uint8_t saa1099_get_map_target_channel(void);
void saa1099_cycle_map_type(void);
void saa1099_cycle_map_type_fast(void);  // Fast cycle: Tone <-> All only
void saa1099_cycle_map_target_channel(void);
const char* saa1099_get_map_status_string(char* buffer, size_t size);

// Ch0 repeat write functions for testing
void saa1099_set_ch0_repeat_count(uint8_t count);
uint8_t saa1099_get_ch0_repeat_count(void);
void saa1099_increase_ch0_repeat_count(void);
void saa1099_decrease_ch0_repeat_count(void);

// Ch0 delay samples control (VGM sample-based delay after ch0 writes)
void saa1099_increase_ch0_delay_samples(void);
void saa1099_decrease_ch0_delay_samples(void);
uint16_t saa1099_get_ch0_delay_samples(void);

// Auto-cycle interval control (in 0x80 command counts)
void saa1099_increase_auto_cycle_interval(void);
void saa1099_decrease_auto_cycle_interval(void);
uint32_t saa1099_get_auto_cycle_interval(void);

// Auto-cycle mode control
void saa1099_cycle_auto_cycle_mode(void);
const char* saa1099_get_auto_cycle_mode_name(void);
void saa1099_on_spfm_delay_0x80(void);  // Called from SPFM layer

#ifdef __cplusplus
}
#endif

#endif