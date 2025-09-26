#ifndef Y8950_H
#define Y8950_H

#include <stdint.h>

void y8950_init(uint8_t slot);
void y8950_mute(uint8_t slot);
void y8950_write_reg(uint8_t slot, uint8_t reg, uint8_t data);

#endif // Y8950_H
