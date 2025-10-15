#ifndef WS_TO_OPM_H
#define WS_TO_OPM_H

#include <stdint.h>
#include "chiptype.h"

// Callback function pointer for writing OPM data
typedef void (*opm_write_func_t)(uint8_t addr, uint8_t data);

void ws_to_opm_init(chip_type_t source_chip_type, uint32_t source_clock, opm_write_func_t write_func);
void ws_to_opm_write_reg(uint8_t port, uint8_t addr, uint8_t data);
void ws_to_opm_update(uint32_t samples); // For time-based updates if needed

#endif /* WS_TO_OPM_H */
