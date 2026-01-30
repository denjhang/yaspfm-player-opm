#ifndef Y8950_H
#define Y8950_H

#include <stdint.h>
#include <stdbool.h>

void y8950_init(uint8_t slot);
void y8950_mute(uint8_t slot);
void y8950_write_reg(uint8_t slot, uint8_t reg, uint8_t data);
uint32_t y8950_write_adpcm_data(uint8_t slot, uint32_t rom_size, uint32_t start_addr, const uint8_t* data, uint32_t data_size);

// Y8950寄存器定义
#define Y8950_FLAG_CONTROL        0x04
#define Y8950_ADPCM_CONTROL       0x07
#define Y8950_START_ADDRESS_L     0x09
#define Y8950_START_ADDRESS_H     0x0A
#define Y8950_STOP_ADDRESS_L      0x0B
#define Y8950_STOP_ADDRESS_H      0x0C
#define Y8950_ADPCM_DATA          0x0F

#endif // Y8950_H
