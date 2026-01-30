#include "adpcm.h"
#include <stdlib.h>
#include <math.h>

static const int F[] = {57, 57, 57, 57, 77, 102, 128, 153};

uint8_t* ym2608_adpcm_encode(int16_t* pcm, int pcm_len, int* adpcm_len) {
    *adpcm_len = pcm_len / 2;
    uint8_t* adpcm = (uint8_t*)malloc(*adpcm_len);
    if (!adpcm) return NULL;

    int x = 0;
    int D = 127;
    uint8_t dh = 0;

    for (int n = 0; n < pcm_len; n++) {
        int d = pcm[n] - x;
        int L4 = d < 0 ? 1 : 0;
        int l = abs(d) * 4 / D;
        int L31 = l > 7 ? 7 : l;

        x += (((1 - 2 * L4) * (L31 * 2 + 1) * D) >> 3);
        D = (F[L31] * D) >> 6;
        if (D > 0x5fff) D = 0x5fff;
        if (D < 127) D = 127;

        if (n & 1) {
            uint8_t dl = (L4 << 3) | L31;
            adpcm[n >> 1] = (dh << 4) | dl;
        } else {
            dh = (L4 << 3) | L31;
        }
    }
    return adpcm;
}

static const int step_table[] = {
    16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552
};

static int16_t M(int step, int nib) {
    const int a = step_table[step];
    const int p = (nib & 8) ? -1 : 1;
    const int l3 = (nib & 4) ? a : 0;
    const int l2 = (nib & 2) ? a >> 1 : 0;
    const int l1 = (nib & 1) ? a >> 2 : 0;
    const int l0 = a >> 3;
    return p * (l3 + l2 + l1 + l0);
}

int16_t* okim6258_adpcm_decode(uint8_t* adpcm, int adpcm_len, int* pcm_len) {
    static const int index_shift[] = {-1, -1, -1, -1, 2, 4, 6, 8};
    *pcm_len = adpcm_len * 2;
    int16_t* pcm = (int16_t*)malloc(*pcm_len * sizeof(int16_t));
    if (!pcm) return NULL;

    int out = 0;
    int step = 0;
    for (int i = 0; i < *pcm_len; i++) {
        int nib = (i & 1) ? (adpcm[i >> 1] >> 4) : (adpcm[i >> 1] & 0xf);
        int sample = M(step, nib);
        out = ((sample << 8) + out * 245) >> 8;
        step += index_shift[nib & 7];
        if (step > 48) step = 48;
        if (step < 0) step = 0;
        pcm[i] = out << 4;
    }
    return pcm;
}
