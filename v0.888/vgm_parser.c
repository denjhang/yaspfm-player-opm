#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// Helper to read little-endian 32-bit integer
static uint32_t read_le32(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

// Helper to read little-endian 16-bit integer
static uint16_t read_le16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

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

} vgm_header_t;

// Helper to read a relative offset
static uint32_t read_rel_ofs(const uint8_t* data, uint32_t base_ofs) {
    uint32_t ofs = read_le32(data + base_ofs);
    return ofs ? (base_ofs + ofs) : 0;
}

void print_header(const vgm_header_t* h) {
    printf("--- VGM Header Info ---\n");
    printf("Ident: %.4s\n", h->ident);
    printf("Version: 0x%08X\n", h->version);
    printf("Data Offset: 0x%08X\n", h->vgm_data_offset);
    printf("Total Samples: %u\n", h->total_samples);
    printf("Loop Offset: 0x%08X\n", h->loop_offset);
    printf("Loop Samples: %u\n", h->loop_samples);
    printf("\n--- Chip Clocks ---\n");
    if (h->sn76489_clock) printf("SN76489: %u Hz\n", h->sn76489_clock & 0x7FFFFFFF);
    if (h->ym2413_clock)  printf("YM2413: %u Hz\n", h->ym2413_clock & 0x7FFFFFFF);
    if (h->ym2612_clock)  printf("YM2612: %u Hz\n", h->ym2612_clock & 0x7FFFFFFF);
    if (h->ym2151_clock)  printf("YM2151: %u Hz\n", h->ym2151_clock & 0x7FFFFFFF);
    if (h->ym2203_clock)  printf("YM2203: %u Hz\n", h->ym2203_clock & 0x7FFFFFFF);
    if (h->ym2608_clock)  printf("YM2608: %u Hz\n", h->ym2608_clock & 0x7FFFFFFF);
    if (h->ym2610_clock)  printf("YM2610: %u Hz\n", h->ym2610_clock & 0x7FFFFFFF);
    if (h->ym3812_clock)  printf("YM3812: %u Hz\n", h->ym3812_clock & 0x7FFFFFFF);
    if (h->ym3526_clock)  printf("YM3526: %u Hz\n", h->ym3526_clock & 0x7FFFFFFF);
    if (h->y8950_clock)   printf("Y8950: %u Hz\n", h->y8950_clock & 0x7FFFFFFF);
    if (h->ymf262_clock)  printf("YMF262: %u Hz\n", h->ymf262_clock & 0x7FFFFFFF);
    if (h->ay8910_clock)  printf("AY8910: %u Hz\n", h->ay8910_clock & 0x7FFFFFFF);
    if (h->sega_pcm_clock) printf("Sega PCM: %u Hz\n", h->sega_pcm_clock & 0x7FFFFFFF);
    if (h->rf5c68_clock)  printf("RF5C68: %u Hz\n", h->rf5c68_clock & 0x7FFFFFFF);
    if (h->rf5c164_clock) printf("RF5C164: %u Hz\n", h->rf5c164_clock & 0x7FFFFFFF);
    if (h->pwm_clock)     printf("PWM: %u Hz\n", h->pwm_clock & 0x7FFFFFFF);
    if (h->gb_dmg_clock)  printf("GameBoy DMG: %u Hz\n", h->gb_dmg_clock & 0x7FFFFFFF);
    if (h->nes_apu_clock) printf("NES APU: %u Hz\n", h->nes_apu_clock & 0x7FFFFFFF);
    if (h->multipcm_clock) printf("MultiPCM: %u Hz\n", h->multipcm_clock & 0x7FFFFFFF);
    if (h->upd7759_clock) printf("uPD7759: %u Hz\n", h->upd7759_clock & 0x7FFFFFFF);
    if (h->okim6258_clock) printf("OKIM6258: %u Hz\n", h->okim6258_clock & 0x7FFFFFFF);
    if (h->okim6295_clock) printf("OKIM6295: %u Hz\n", h->okim6295_clock & 0x7FFFFFFF);
    if (h->k051649_clock) printf("K051649: %u Hz\n", h->k051649_clock & 0x7FFFFFFF);
    if (h->k054539_clock) printf("K054539: %u Hz\n", h->k054539_clock & 0x7FFFFFFF);
    if (h->huc6280_clock) printf("HuC6280: %u Hz\n", h->huc6280_clock & 0x7FFFFFFF);
    if (h->c140_clock)    printf("C140: %u Hz\n", h->c140_clock & 0x7FFFFFFF);
    if (h->k053260_clock) printf("K053260: %u Hz\n", h->k053260_clock & 0x7FFFFFFF);
    if (h->pokey_clock)   printf("Pokey: %u Hz\n", h->pokey_clock & 0x7FFFFFFF);
    if (h->qsound_clock)  printf("QSound: %u Hz\n", h->qsound_clock & 0x7FFFFFFF);
    if (h->scsp_clock)    printf("SCSP: %u Hz\n", h->scsp_clock & 0x7FFFFFFF);
    if (h->wonderswan_clock) printf("WonderSwan: %u Hz\n", h->wonderswan_clock & 0x7FFFFFFF);
    if (h->vsu_clock)     printf("VSU: %u Hz\n", h->vsu_clock & 0x7FFFFFFF);
    if (h->saa1099_clock) printf("SAA1099: %u Hz\n", h->saa1099_clock & 0x7FFFFFFF);
    if (h->es5503_clock)  printf("ES5503: %u Hz\n", h->es5503_clock & 0x7FFFFFFF);
    if (h->es5506_clock)  printf("ES5506: %u Hz\n", h->es5506_clock & 0x7FFFFFFF);
    if (h->x1_010_clock)  printf("X1-010: %u Hz\n", h->x1_010_clock & 0x7FFFFFFF);
    if (h->c352_clock)    printf("C352: %u Hz\n", h->c352_clock & 0x7FFFFFFF);
    if (h->ga20_clock)    printf("GA20: %u Hz\n", h->ga20_clock & 0x7FFFFFFF);
    printf("-----------------------\n");
}

bool parse_vgm_header(FILE* fp, vgm_header_t* header) {
    uint8_t hdr_buf[0x100];
    memset(header, 0, sizeof(vgm_header_t));

    // Read the initial part of the header to get the version
    if (fseek(fp, 0, SEEK_SET) != 0 || fread(hdr_buf, 1, 0x40, fp) != 0x40) {
        fprintf(stderr, "Error: Could not read first 0x40 bytes of header.\n");
        return false;
    }

    if (memcmp(hdr_buf, "Vgm ", 4) != 0) {
        fprintf(stderr, "Error: Invalid VGM signature.\n");
        return false;
    }
    memcpy(header->ident, hdr_buf, 4);

    header->version = read_le32(hdr_buf + 0x08);
    
    // Determine how much of the full 256-byte header to read from the file
    size_t header_size_to_read = 0;
    if (header->version >= 0x171) header_size_to_read = 0xE4;
    else if (header->version >= 0x170) header_size_to_read = 0xC0;
    else if (header->version >= 0x161) header_size_to_read = 0xB8;
    else if (header->version >= 0x151) header_size_to_read = 0x80;
    else header_size_to_read = 0x40;

    // Read the rest of the available header if needed
    if (header_size_to_read > 0x40) {
        if (fread(hdr_buf + 0x40, 1, header_size_to_read - 0x40, fp) != (header_size_to_read - 0x40)) {
            fprintf(stderr, "Warning: Could not read full extended header. Some data may be missing.\n");
        }
    }

    // --- Parse all fields based on version ---
    header->eof_offset = read_le32(hdr_buf + 0x04);
    header->total_samples = read_le32(hdr_buf + 0x18);
    header->loop_offset = read_rel_ofs(hdr_buf, 0x1C);
    header->loop_samples = read_le32(hdr_buf + 0x20);
    
    if (header->version >= 0x150) {
        header->vgm_data_offset = read_rel_ofs(hdr_buf, 0x34);
    } else {
        header->vgm_data_offset = 0x40;
    }
    if (header->vgm_data_offset == 0) header->vgm_data_offset = 0x40;

    // Clocks and other data, checking version for validity
    header->sn76489_clock = read_le32(hdr_buf + 0x0C);
    header->ym2413_clock = read_le32(hdr_buf + 0x10);

    if (header->version >= 0x101) {
        header->rate = read_le32(hdr_buf + 0x24);
    }
    if (header->version >= 0x110) {
        header->sn76489_feedback = read_le16(hdr_buf + 0x28);
        header->sn76489_shift_width = hdr_buf[0x2A];
        header->ym2612_clock = read_le32(hdr_buf + 0x2C);
        header->ym2151_clock = read_le32(hdr_buf + 0x30);
    }
    if (header->version >= 0x151) {
        header->sn76489_flags = hdr_buf[0x2B];
        header->sega_pcm_clock = read_le32(hdr_buf + 0x38);
        header->spcm_interface = read_le32(hdr_buf + 0x3C);
        header->rf5c68_clock = read_le32(hdr_buf + 0x40);
        header->ym2203_clock = read_le32(hdr_buf + 0x44);
        header->ym2608_clock = read_le32(hdr_buf + 0x48);
        header->ym2610_clock = read_le32(hdr_buf + 0x4C);
        header->ym3812_clock = read_le32(hdr_buf + 0x50);
        header->ym3526_clock = read_le32(hdr_buf + 0x54);
        header->y8950_clock = read_le32(hdr_buf + 0x58);
        header->ymf262_clock = read_le32(hdr_buf + 0x5C);
        header->ymf278b_clock = read_le32(hdr_buf + 0x60);
        header->ymf271_clock = read_le32(hdr_buf + 0x64);
        header->ymz280b_clock = read_le32(hdr_buf + 0x68);
        header->rf5c164_clock = read_le32(hdr_buf + 0x6C);
        header->pwm_clock = read_le32(hdr_buf + 0x70);
        header->ay8910_clock = read_le32(hdr_buf + 0x74);
        header->ay8910_chip_type = hdr_buf[0x78];
        header->ay8910_flags = hdr_buf[0x79];
        header->ym2203_ay8910_flags = hdr_buf[0x7A];
        header->ym2608_ay8910_flags = hdr_buf[0x7B];
        header->loop_modifier = hdr_buf[0x7F];
    }
     if (header->version >= 0x160) {
        header->volume_modifier = hdr_buf[0x7C];
        header->loop_base = hdr_buf[0x7E];
    }
    if (header->version >= 0x161) {
        header->gb_dmg_clock = read_le32(hdr_buf + 0x80);
        header->nes_apu_clock = read_le32(hdr_buf + 0x84);
        header->multipcm_clock = read_le32(hdr_buf + 0x88);
        header->upd7759_clock = read_le32(hdr_buf + 0x8C);
        header->okim6258_clock = read_le32(hdr_buf + 0x90);
        header->okim6258_flags = hdr_buf[0x94];
        header->k054539_flags = hdr_buf[0x95];
        header->c140_chip_type = hdr_buf[0x96];
        header->okim6295_clock = read_le32(hdr_buf + 0x98);
        header->k051649_clock = read_le32(hdr_buf + 0x9C);
        header->k054539_clock = read_le32(hdr_buf + 0xA0);
        header->huc6280_clock = read_le32(hdr_buf + 0xA4);
        header->c140_clock = read_le32(hdr_buf + 0xA8);
        header->k053260_clock = read_le32(hdr_buf + 0xAC);
        header->pokey_clock = read_le32(hdr_buf + 0xB0);
        header->qsound_clock = read_le32(hdr_buf + 0xB4);
    }
    if (header->version >= 0x170) {
        header->extra_header_offset = read_rel_ofs(hdr_buf, 0xBC);
    }
    if (header->version >= 0x171) {
        header->scsp_clock = read_le32(hdr_buf + 0xB8);
        header->wonderswan_clock = read_le32(hdr_buf + 0xC0);
        header->vsu_clock = read_le32(hdr_buf + 0xC4);
        header->saa1099_clock = read_le32(hdr_buf + 0xC8);
        header->es5503_clock = read_le32(hdr_buf + 0xCC);
        header->es5506_clock = read_le32(hdr_buf + 0xD0);
        header->es5503_channels = hdr_buf[0xD4];
        header->es5505_6_channels = hdr_buf[0xD5];
        header->c352_clock_divider = hdr_buf[0xD6];
        header->x1_010_clock = read_le32(hdr_buf + 0xD8);
        header->c352_clock = read_le32(hdr_buf + 0xDC);
        header->ga20_clock = read_le32(hdr_buf + 0xE0);
    }

    return true;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <vgm_file>\n", argv[0]);
        return 1;
    }

    // Redirect stdout to a file
    if (freopen("vgm_info.txt", "w", stdout) == NULL) {
        perror("Error redirecting stdout");
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    vgm_header_t header;
    if (parse_vgm_header(fp, &header)) {
        print_header(&header);
    }

    fclose(fp);
    // stdout is closed by freopen, no need to close it again
    return 0;
}
