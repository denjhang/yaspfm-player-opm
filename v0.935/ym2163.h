#ifndef YM2163_H
#define YM2163_H

#include <stdint.h>

// YM2163 (OPP - FM Operator Type-P) is a 4-channel FM synthesizer
// Originally used in Yamaha PSS series keyboards
// Uses serial communication protocol (not parallel register interface)

// YM2163 Register addresses (with D7=1 to indicate address)
#define YM2163_REG_FREQ_LOW_CH0     0x80    // Frequency Low (DV[4:0], DV[1/2], DV[1/4])
#define YM2163_REG_FREQ_LOW_CH1     0x81
#define YM2163_REG_FREQ_LOW_CH2     0x82
#define YM2163_REG_FREQ_LOW_CH3     0x83

#define YM2163_REG_FREQ_HIGH_CH0    0x84    // Frequency High, Octave, Key-On (KON[6], Octave[4:3], F-NUM[10:8])
#define YM2163_REG_FREQ_HIGH_CH1    0x85
#define YM2163_REG_FREQ_HIGH_CH2    0x86
#define YM2163_REG_FREQ_HIGH_CH3    0x87

#define YM2163_REG_TIMBRE_CH0       0x88    // Timbre/Envelope/Waveform (E[2:1], SUS, W[3:1])
#define YM2163_REG_TIMBRE_CH1       0x89
#define YM2163_REG_TIMBRE_CH2       0x8A
#define YM2163_REG_TIMBRE_CH3       0x8B

#define YM2163_REG_VOLUME_CH0       0x8C    // Volume & Output Routing (VL[2:1], F[4:1])
#define YM2163_REG_VOLUME_CH1       0x8D
#define YM2163_REG_VOLUME_CH2       0x8E
#define YM2163_REG_VOLUME_CH3       0x8F

#define YM2163_REG_RHYTHM           0x90    // Rhythm Trigger (BD, HC, SDN, HHO/HHD)

#define YM2163_REG_RHYTHM_VOL_BD    0x94    // Rhythm Volume
#define YM2163_REG_RHYTHM_VOL_HC    0x95
#define YM2163_REG_RHYTHM_VOL_SDN   0x96
#define YM2163_REG_RHYTHM_VOL_HH    0x97

// Key-On bit position in FREQ_HIGH register
#define YM2163_KEY_ON_BIT           0x40

// Number of channels
#define YM2163_NUM_CHANNELS         4

// Function prototypes
void ym2163_init(uint8_t slot);
void ym2163_mute(uint8_t slot);
void ym2163_write_reg(uint8_t slot, uint8_t addr, uint8_t data);
void ym2163_note_on(uint8_t slot, uint8_t channel, uint8_t note, uint8_t velocity, uint8_t timbre);
void ym2163_note_off(uint8_t slot, uint8_t channel);
void ym2163_set_frequency(uint8_t slot, uint8_t channel, uint16_t fnum, uint8_t octave);
void ym2163_set_timbre(uint8_t slot, uint8_t channel, uint8_t timbre);
void ym2163_set_volume(uint8_t slot, uint8_t channel, uint8_t volume);

#endif // YM2163_H
