#ifndef AY_TO_OPM_H
#define AY_TO_OPM_H

#include <stdint.h>
#include "chiptype.h"

// AY Stereo Panning Modes
typedef enum {
    AY_STEREO_ABC, // A=L, B=C, C=R
    AY_STEREO_ACB, // A=L, C=C, B=R
    AY_STEREO_BAC, // B=L, A=C, C=R
    AY_STEREO_MONO, // All Center
    AY_STEREO_MODE_COUNT
} ay_stereo_mode_t;

// Callback function pointer for writing OPM data
typedef void (*opm_write_func_t)(uint8_t addr, uint8_t data);

void ay_to_opm_init(chip_type_t source_chip_type, uint32_t source_clock, opm_write_func_t write_func);
void ay_to_opm_write_reg(uint8_t addr, uint8_t data);
void ay_to_opm_update_envelope(void);
void ay_to_opm_set_stereo_mode(ay_stereo_mode_t mode);
const char* ay_to_opm_get_stereo_mode_name(ay_stereo_mode_t mode);


#endif /* AY_TO_OPM_H */
