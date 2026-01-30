#ifndef SN76489_H
#define SN76489_H

#include <stdint.h>

void sn76489_init(uint8_t slot);
void sn76489_mute(uint8_t slot);
void sn76489_write_reg(uint8_t slot, uint8_t data);

#endif // SN76489_H
