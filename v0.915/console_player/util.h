/* See LICENSE for licence details. */
#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 1024
#endif

extern volatile int g_timer_mode;

int read_2byte_le(FILE *fp, uint16_t *value);
int read_4byte_le(FILE *fp, uint32_t *value);
uint64_t read_variable_length_7bit_le(FILE *fp);
bool read_string(FILE *fp, char *str, int size);
uint8_t low_byte(uint32_t value);
uint8_t high_byte(uint32_t value);

uint32_t read_le32(const uint8_t* ptr);

void yasp_timer_init(void);
void yasp_timer_release(void);
void yasp_usleep(uint64_t usec);
void yasp_usleep_precise(unsigned int usec);
uint64_t get_current_time_us(void);
unsigned long yasp_get_tick_count(void);

void clear_screen();
int get_single_char();

#endif /* UTIL_H */
