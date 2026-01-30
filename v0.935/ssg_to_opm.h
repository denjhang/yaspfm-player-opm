#ifndef SSG_TO_OPM_H
#define SSG_TO_OPM_H

#include <stdint.h>
#include "chiptype.h"
#include "ay_to_opm.h" // For opm_write_func_t and stereo mode

// OPM has 8 channels. OPN uses up to 6.
// We can use the remaining channels for SSG.
// Let's define a base channel to avoid conflicts.
// OPN uses 0-5. AY uses 4-7.
// To be safe, let's analyze channel usage.
// YM2203: 3 FM + 3 SSG -> 3 OPM + 3 OPM (Total 6)
// YM2608: 6 FM + 3 SSG -> 6 OPM + 3 OPM (Total 9, exceeds OPM's 8 channels)
// We need a strategy for channel allocation, especially for YM2608.

// For now, let's assume we have at least 3 channels available.
// We will implement dynamic allocation later.

typedef void (*opm_write_func_t)(uint8_t addr, uint8_t data);

void ssg_to_opm_init(chip_type_t source_chip_type, uint32_t source_clock, opm_write_func_t write_func);
void ssg_to_opm_write_reg(uint8_t addr, uint8_t data);
void ssg_to_opm_update_envelope(void);
void ssg_to_opm_set_stereo_mode(ay_stereo_mode_t mode);
void ssg_to_opm_set_fixed_mapping(const int* ssg_channels, const int* opm_channels, int count);
const char* ssg_to_opm_get_stereo_mode_name(ay_stereo_mode_t mode);
ay_stereo_mode_t ssg_to_opm_cycle_stereo_mode(void);
ay_stereo_mode_t ssg_to_opm_get_current_stereo_mode(void);

#endif // SSG_TO_OPM_H
