#ifndef SN_TO_AY_H
#define SN_TO_AY_H

#include <stdint.h>
#include "chiptype.h"

void sn_to_ay_init(chip_type_t source_chip_type, uint32_t source_clock);
void sn_to_ay_write_reg(uint8_t data);

#endif /* SN_TO_AY_H */
