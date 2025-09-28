#ifndef VGM_H
#define VGM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define VGM_DEFAULT_WAIT1 735
#define VGM_DEFAULT_WAIT2 882
#define VGM_SAMPLE_RATE 44100

// A comprehensive VGM header structure based on vgmspec171.txt
typedef struct {
    // 0x00
    char ident[4];
    uint32_t eof_offset;
    uint32_t version;
    uint32_t sn76489_clock;
    // 0x10
    uint32_t ym2413_clock;
    uint32_t gd3_offset;
    uint32_t total_samples;
    uint32_t loop_offset;
    // 0x20
    uint32_t loop_samples;
    uint32_t rate;
    // 0x28
    uint16_t sn76489_feedback;
    uint8_t sn76489_shift_width;
    uint8_t sn76489_flags;
    uint32_t ym2612_clock;
    // 0x30
    uint32_t ym2151_clock;
    uint32_t vgm_data_offset;
    // 0x38
    uint32_t sega_pcm_clock;
    uint32_t spcm_interface;
    // 0x40
    uint32_t rf5c68_clock;
    uint32_t ym2203_clock;
    uint32_t ym2608_clock;
    uint32_t ym2610_clock;
    // 0x50
    uint32_t ym3812_clock;
    uint32_t ym3526_clock;
    uint32_t y8950_clock;
    uint32_t ymf262_clock;
    // 0x60
    uint32_t ymf278b_clock;
    uint32_t ymf271_clock;
    uint32_t ymz280b_clock;
    uint32_t rf5c164_clock;
    // 0x70
    uint32_t pwm_clock;
    uint32_t ay8910_clock;
    uint8_t ay8910_chip_type;
    uint8_t ay8910_flags;
    uint8_t ym2203_ay8910_flags;
    uint8_t ym2608_ay8910_flags;
    // 0x7c
    uint8_t volume_modifier;
    uint8_t reserved_7d;
    uint8_t loop_base;
    uint8_t loop_modifier;
    // 0x80
    uint32_t gb_dmg_clock;
    uint32_t nes_apu_clock;
    uint32_t multipcm_clock;
    uint32_t upd7759_clock;
    // 0x90
    uint32_t okim6258_clock;
    uint8_t okim6258_flags;
    uint8_t k054539_flags;
    uint8_t c140_chip_type;
    uint8_t reserved_97;
    uint32_t okim6295_clock;
    // 0x9c
    uint32_t k051649_clock;
    // 0xA0
    uint32_t k054539_clock;
    uint32_t huc6280_clock;
    uint32_t c140_clock;
    uint32_t k053260_clock;
    // 0xB0
    uint32_t pokey_clock;
    uint32_t qsound_clock;
    // 0xB8
    uint32_t scsp_clock;
    uint32_t extra_header_offset;
    // 0xC0
    uint32_t wonderswan_clock;
    uint32_t vsu_clock;
    uint32_t saa1099_clock;
    uint32_t es5503_clock;
    // 0xD0
    uint32_t es5506_clock;
    uint8_t es5503_channels;
    uint8_t es5505_6_channels;
    uint8_t c352_clock_divider;
    uint8_t reserved_d7;
    uint32_t x1_010_clock;
    // 0xDC
    uint32_t c352_clock;
    // 0xE0
    uint32_t ga20_clock;

    // GD3 Tag
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

extern vgm_header_t g_vgm_header;
extern volatile bool g_is_playing;

bool vgm_parse_header(FILE *fp, vgm_header_t *header);
FILE* vgm_play(FILE *fp, const char *filename, const char *cache_filename);
DWORD WINAPI vgm_player_thread(LPVOID lpParam);

#endif // VGM_H
