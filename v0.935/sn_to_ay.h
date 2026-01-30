#ifndef SN_TO_AY_H
#define SN_TO_AY_H

#include <stdint.h>
#include "chiptype.h"

// Callback function pointer for writing AY8910 data
typedef void (*ay_write_func_t)(uint8_t addr, uint8_t data);

void sn_to_ay_init(chip_type_t source_chip_type, uint32_t source_clock, ay_write_func_t write_func);
void sn_to_ay_write_reg(uint8_t data);

#endif /* SN_TO_AY_H */
