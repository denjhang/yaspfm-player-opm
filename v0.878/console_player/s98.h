#ifndef S98_H
#define S98_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t* buffer;
    uint32_t size;
    uint32_t pos;
    uint8_t version;
    uint32_t timer_info;
    uint32_t timer_info2;
    uint32_t compressing;
    uint32_t offset_to_dump;
    uint32_t offset_to_loop;
    uint32_t device_count;
} S98;

bool s98_load(S98* s98, FILE *fp);
bool s98_parse_header(S98* s98);
bool s98_play(S98* s98, const char *filename);
void s98_release(S98* s98);

#endif /* S98_H */
