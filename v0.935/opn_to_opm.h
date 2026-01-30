#ifndef OPN_TO_OPM_H
#define OPN_TO_OPM_H

#include <stdint.h>
#include "chiptype.h"

// Callback function pointer for writing converted OPM data
typedef void (*opm_write_func_t)(uint8_t addr, uint8_t data);

// Initializes the converter for a specific OPN chip type
void opn_to_opm_init(chip_type_t source_chip_type, uint32_t source_clock, opm_write_func_t write_func);

// Converts and processes an OPN register write
void opn_to_opm_write_reg(uint8_t addr, uint8_t data, uint8_t port);

#endif // OPN_TO_OPM_H
