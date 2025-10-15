#ifndef VGM_H
#define VGM_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "chiptype.h"

#define VGM_SAMPLE_RATE 44100
#define VGM_DEFAULT_WAIT1 735
#define VGM_DEFAULT_WAIT2 882

typedef struct {
    char ident[4];
    uint32_t eof_offset;
    uint32_t version;
    uint32_t sn76489_clock;
    uint32_t ym2413_clock;
    uint32_t gd3_offset;
    uint32_t total_samples;
    uint32_t loop_offset;
    uint32_t loop_samples;
    uint32_t rate;
    uint16_t sn76489_feedback;
    uint8_t sn76489_shift_width;
    uint8_t sn76489_flags;
    uint32_t ym2612_clock;
    uint32_t ym2151_clock;
    uint32_t vgm_data_offset;
    uint32_t sega_pcm_clock;
    uint32_t spcm_interface;
    uint32_t rf5c68_clock;
    uint32_t ym2203_clock;
    uint32_t ym2608_clock;
    uint32_t ym2610_clock;
    uint32_t ym3812_clock;
    uint32_t ym3526_clock;
    uint32_t y8950_clock;
    uint32_t ymf262_clock;
    uint32_t ymf278b_clock;
    uint32_t ymf271_clock;
    uint32_t ymz280b_clock;
    uint32_t rf5c164_clock;
    uint32_t pwm_clock;
    uint32_t ay8910_clock;
    uint8_t ay8910_chip_type;
    uint8_t ay8910_flags;
    uint8_t ay8910_ym2203_flags;
    uint8_t ay8910_ym2608_flags;
    uint8_t volume_modifier;
    uint8_t reserved_1;
    uint8_t loop_base;
    uint8_t loop_modifier;
    uint32_t gb_dmg_clock;
    uint32_t nes_apu_clock;
    uint32_t multipcm_clock;
    uint32_t upd7759_clock;
    uint32_t okim6258_clock;
    uint8_t okim6258_flags;
    uint8_t k054539_flags;
    uint8_t c140_chip_type;
    uint8_t reserved_2;
    uint32_t okim6295_clock;
    uint32_t k051649_clock;
    uint32_t k054539_clock;
    uint32_t huc6280_clock;
    uint32_t c140_clock;
    uint32_t k053260_clock;
    uint32_t pokey_clock;
    uint32_t qsound_clock;
    uint32_t scsp_clock;
    uint32_t wonderswan_clock;
    uint32_t vsu_clock;
    uint32_t saa1099_clock;
    uint32_t es5503_clock;
    uint32_t es5506_clock;
    uint8_t es5503_channels;
    uint8_t es5506_channels;
    uint8_t c352_clock_divider;
    uint8_t reserved_3;
    uint32_t x1_010_clock;
    uint32_t c352_clock;
    uint32_t ga20_clock;
    // GD3 tag data
    wchar_t track_name_en[256];
    wchar_t track_name_jp[256];
    wchar_t game_name_en[256];
    wchar_t game_name_jp[256];
    wchar_t system_name_en[256];
    wchar_t system_name_jp[256];
    wchar_t author_en[256];
    wchar_t author_jp[256];
    wchar_t release_date[256];
    wchar_t vgm_creator[256];
    wchar_t notes[256];
} vgm_header_t;

extern chip_type_t g_vgm_chip_type;
extern chip_type_t g_original_vgm_chip_type;
extern uint32_t g_original_vgm_chip_clock;
extern vgm_header_t g_vgm_header;

bool vgm_parse_header(FILE* fp, vgm_header_t* header);
FILE* vgm_play(FILE *input_fp, const char *filename, const char *cache_filename, bool force_reconvert);
int vgm_process_command(FILE *input_fp, int* vgm_wait1, int* vgm_wait2, int* loop_counter);
DWORD WINAPI vgm_player_thread(LPVOID lpParam);

#endif // VGM_H
