/* See LICENSE for licence details. */
#ifndef VGM_H
#define VGM_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

enum vgm_misc_t {
	VGM_HEADER_SIZE         = 0x100,
	VGM_SAMPLE_RATE         = 44100,
	VGM_DUAL_CHIP_SUPPORT   = 0x40000000,
	VGM_DATA_OFFSET_FROM    = 0x34,
	VGM_DEFAULT_DATA_OFFSET = 0x40,
	VGM_DEFAULT_WAIT1 = 735,
	VGM_DEFAULT_WAIT2 = 882,
};

struct vgm_header_t {
	char magic[4];
	uint32_t EOF_offset, version, SN76489_clock,
		YM2413_clock, GD3_offset, total_samples, loop_offset,
		loop_samples;
	uint32_t sample_rate;
	uint16_t SN76489_feedback;
	uint8_t SN76489_shift_register_width;
	uint8_t SN76489_flags;
	uint32_t YM2612_clock, YM2151_clock;
	uint32_t VGM_data_offset;
	uint32_t SEGA_PCM_clock, SEGA_PCM_interface_register,
		RF5C68_clock, YM2203_clock, YM2608_clock, YM2610B_clock,
		YM3812_clock, YM3526_clock, YM8950_clock, YMF262_clock,
		YM278B_clock, YMF271_clock, YMZ280B_clock, RF5C164_clock,
		PWM_clock, AY8910_clock;
	uint8_t AY8910_chip_type, AY8910_flags, YM2203_AY8910_flags, YM2608_AY8910_flags;
	uint8_t volume_modifier, reserved0, loop_base;
	uint8_t loop_modifier;
	uint32_t GameBoy_DMG_clock, NES_APU_clock, Multi_PCM_clock, uPD7759_clock,
		OKIM6258_clock;
	uint8_t OKIM6258_flags, K054539_flags, C140_chip_type, reserved1;
	uint32_t OKIM6295_clock, K051649_clock,
		K054539_clock, HuC6280_clock, C140_clock, K053260_clock,
		POKEY_clock, QSound_clock, reserved2;
	uint32_t extra_header_offset, reserved3[16];
};

bool vgm_play(FILE *input_fp, const char *filename);
bool vgm_parse_header(FILE *fp, struct vgm_header_t *header);
int vgm_process_command(FILE *input_fp, struct vgm_header_t* header, int* vgm_wait1, int* vgm_wait2);

#endif /* VGM_H */
