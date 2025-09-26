#ifndef CHIPTYPE_H
#define CHIPTYPE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CHIP_TYPE_NONE,
    CHIP_TYPE_YM2608,
    CHIP_TYPE_YM2151,
    CHIP_TYPE_YM2612,
    CHIP_TYPE_YM2203,
    CHIP_TYPE_YM2413,
    CHIP_TYPE_YM3526,
    CHIP_TYPE_YM3812,
    CHIP_TYPE_Y8950,
    CHIP_TYPE_AY8910,
    CHIP_TYPE_SN76489,
    CHIP_TYPE_YMF262,
    CHIP_TYPE_COUNT
} chip_type_t;

typedef struct {
    chip_type_t type;
    uint8_t slot;
} chip_config_t;

// Global chip configuration
// This will be populated at runtime based on user input or a config file.
extern chip_config_t g_chip_config[CHIP_TYPE_COUNT];

// Function to get the slot for a given chip type
uint8_t get_slot_for_chip(chip_type_t type);

// Function to convert chip type to string
const char* chip_type_to_string(chip_type_t type);
chip_type_t string_to_chip_type(const char* s);

// Function to initialize the chip configuration
void init_chip_config();

#endif // CHIPTYPE_H
