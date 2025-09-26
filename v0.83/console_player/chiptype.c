#include "chiptype.h"
#include <string.h>
#include <stdlib.h>

chip_config_t g_chip_config[CHIP_TYPE_COUNT];

// Map chip type enum to string representation
const char* chip_type_to_string(chip_type_t type) {
    switch (type) {
        case CHIP_TYPE_YM2608: return "YM2608";
        case CHIP_TYPE_YM2151: return "YM2151";
        case CHIP_TYPE_YM2612: return "YM2612";
        case CHIP_TYPE_YM2203: return "YM2203";
        case CHIP_TYPE_YM2413: return "YM2413";
        case CHIP_TYPE_YM3526: return "YM3526";
        case CHIP_TYPE_YM3812: return "YM3812";
        case CHIP_TYPE_Y8950: return "Y8950";
        case CHIP_TYPE_AY8910: return "AY8910";
        case CHIP_TYPE_SN76489: return "SN76489";
        case CHIP_TYPE_YMF262: return "YMF262";
        default: return "NONE";
    }
}

// Map string representation to chip type enum
chip_type_t string_to_chip_type(const char* str) {
    if (strcmp(str, "YM2608") == 0) return CHIP_TYPE_YM2608;
    if (strcmp(str, "YM2151") == 0) return CHIP_TYPE_YM2151;
    if (strcmp(str, "YM2612") == 0) return CHIP_TYPE_YM2612;
    if (strcmp(str, "YM2203") == 0) return CHIP_TYPE_YM2203;
    if (strcmp(str, "YM2413") == 0) return CHIP_TYPE_YM2413;
    if (strcmp(str, "YM3526") == 0) return CHIP_TYPE_YM3526;
    if (strcmp(str, "YM3812") == 0) return CHIP_TYPE_YM3812;
    if (strcmp(str, "Y8950") == 0) return CHIP_TYPE_Y8950;
    if (strcmp(str, "AY8910") == 0) return CHIP_TYPE_AY8910;
    if (strcmp(str, "SN76489") == 0) return CHIP_TYPE_SN76489;
    if (strcmp(str, "YMF262") == 0) return CHIP_TYPE_YMF262;
    return CHIP_TYPE_NONE;
}

uint8_t get_slot_for_chip(chip_type_t type) {
    for (int i = 0; i < CHIP_TYPE_COUNT; i++) {
        if (g_chip_config[i].type == type) {
            return g_chip_config[i].slot;
        }
    }
    return 0xFF; // Return invalid slot if not found
}
