#ifndef YMF262_H
#define YMF262_H

#include <stdint.h>

void ymf262_init(uint8_t slot);
void ymf262_mute(uint8_t slot);
void ymf262_write_reg(uint8_t slot, uint8_t port, uint8_t reg, uint8_t data);

#endif // YMF262_H
