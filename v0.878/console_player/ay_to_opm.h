#ifndef AY_TO_OPM_H
#define AY_TO_OPM_H

#include <stdint.h>
#include "chiptype.h"

// Callback function pointer for writing OPM data
typedef void (*opm_write_func_t)(uint8_t addr, uint8_t data);

void ay_to_opm_init(chip_type_t source_chip_type, uint32_t source_clock, opm_write_func_t write_func);
void ay_to_opm_write_reg(uint8_t addr, uint8_t data);

#endif /* AY_TO_OPM_H */
