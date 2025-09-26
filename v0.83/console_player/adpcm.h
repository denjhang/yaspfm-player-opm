#ifndef ADPCM_H
#define ADPCM_H

#include <stdint.h>

uint8_t* ym2608_adpcm_encode(int16_t* pcm, int pcm_len, int* adpcm_len);
int16_t* okim6258_adpcm_decode(uint8_t* adpcm, int adpcm_len, int* pcm_len);

#endif // ADPCM_H
