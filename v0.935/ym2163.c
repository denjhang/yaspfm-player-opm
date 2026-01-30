#include "ym2163.h"
#include "spfm.h"
#include <math.h>

// YM2163 (OPP - FM Operator Type-P)
// 4-channel FM synthesizer

// Channel state tracking
static struct {
    uint16_t fnum;
    uint8_t octave;
    uint8_t key_on;
} channel_state[2][YM2163_NUM_CHANNELS]; // [slot][channel]

// Frequency table from MARS21 project (Z-80 ASM code)
// Format: lower 13 bits = F-NUM, upper 3 bits = octave (embedded in data)
static const uint16_t FNUM_TABLE[128] = {
    // MIDI notes 35-85 from MIA42.asm
    [35] = 0x0737, [36] = 0x0702, [37] = 0x064F, [38] = 0x0620,
    [39] = 0x0573, [40] = 0x0549, [41] = 0x0521, [42] = 0x047B,
    [43] = 0x0457, [44] = 0x0436, [45] = 0x0416, [46] = 0x0770,
    [47] = 0x0F70, [48] = 0x0F37, [49] = 0x0F02, [50] = 0x0E4F,
    [51] = 0x0E20, [52] = 0x0D73, [53] = 0x0D49, [54] = 0x0D21,
    [55] = 0x0C7B, [56] = 0x0C57, [57] = 0x0C36, [58] = 0x0C16,
    [59] = 0x1770, [60] = 0x1737, [61] = 0x1702, [62] = 0x164F,
    [63] = 0x1620, [64] = 0x1573, [65] = 0x1549, [66] = 0x1521,
    [67] = 0x147B, [68] = 0x1457, [69] = 0x1436, [70] = 0x1416,
    [71] = 0x1F70, [72] = 0x1F37, [73] = 0x1F02, [74] = 0x1E4F,
    [75] = 0x1E20, [76] = 0x1D73, [77] = 0x1D49, [78] = 0x1D21,
    [79] = 0x1C7B, [80] = 0x1C57, [81] = 0x1C36, [82] = 0x1C16,
    [83] = 0x1B78, [84] = 0x1B5C, [85] = 0x1B41
};

// Low-level register write (YM2151-style protocol)
// YM2163 uses same protocol as YM2151: port 0, addresses 0x80-0x9F
void ym2163_write_reg(uint8_t slot, uint8_t addr, uint8_t data) {
    // YM2151-style: direct write to port 0 with full address
    spfm_write_reg(slot, 0, addr, data);
}

// Initialize YM2163 to clean state
void ym2163_init(uint8_t slot) {
    // Initialize channel state
    for (int ch = 0; ch < YM2163_NUM_CHANNELS; ch++) {
        channel_state[slot][ch].fnum = 0;
        channel_state[slot][ch].octave = 0;
        channel_state[slot][ch].key_on = 0;
    }

    // Mute all channels first
    ym2163_mute(slot);

    // Initialize registers using MIA42 standard values
    // Volume register format (8CH~8FH):
    //   Bits 5-4: VL2,VL1 (00=0dB, 01=-6dB, 10=-12dB, 11=-∞dB/mute)
    //   Bits 3-0: F3,F2,F1 output routing (1=enable output to OR1/OR2/OR3)
    // Use 0x07: VL=00 (0dB max volume) + F3F2F1=111 (OR1/OR2/OR3 outputs)
    for (int ch = 0; ch < YM2163_NUM_CHANNELS; ch++) {
        ym2163_write_reg(slot, YM2163_REG_VOLUME_CH0 + ch, 0x07); // MIA42 standard value
    }

    // Set default timbre for all channels (Strings: E=1, W=1)
    for (int ch = 0; ch < YM2163_NUM_CHANNELS; ch++) {
        ym2163_write_reg(slot, YM2163_REG_TIMBRE_CH0 + ch, 0x11);
    }

    // Disable rhythm mode
    ym2163_write_reg(slot, YM2163_REG_RHYTHM, 0x00);

    spfm_flush();
}

// Mute all channels
void ym2163_mute(uint8_t slot) {
    // Key off all channels and set volume to minimum
    for (int ch = 0; ch < YM2163_NUM_CHANNELS; ch++) {
        channel_state[slot][ch].key_on = 0;

        // Key off (clear KON bit)
        // freq_msb = bits 0-4 (5 bits), octave = bits 5-6 (2 bits)
        uint8_t freq_msb = (channel_state[slot][ch].fnum >> 8) & 0x1F;
        uint8_t octave_bits = (channel_state[slot][ch].octave & 0x03) << 5;
        uint8_t freq_high = octave_bits | freq_msb;
        ym2163_write_reg(slot, YM2163_REG_FREQ_HIGH_CH0 + ch, freq_high);

        // Set volume to mute (VL=3 = -∞dB)
        // 0x3F: VL=11 (mute) + F3F2F1=1111 (all outputs, but muted anyway)
        ym2163_write_reg(slot, YM2163_REG_VOLUME_CH0 + ch, 0x3F);
    }

    // Disable rhythm
    ym2163_write_reg(slot, YM2163_REG_RHYTHM, 0x00);

    spfm_flush();
}

// Set frequency for a channel
void ym2163_set_frequency(uint8_t slot, uint8_t channel, uint16_t fnum, uint8_t octave) {
    if (channel >= YM2163_NUM_CHANNELS) return;

    channel_state[slot][channel].fnum = fnum;
    channel_state[slot][channel].octave = octave;

    // YM2163 uses 13-bit F-NUM + 2-bit octave (based on MIA42.asm)
    // Register 0x00-0x03: 8-bit frequency LSB
    // Register 0x04-0x07 bits 0-4: 5-bit frequency MSB (13 bits total)
    // Register 0x04-0x07 bits 5-6: 2-bit octave (0-3)
    // Register 0x04-0x07 bit 6 in final write: KON (Key ON)

    // Write frequency LSB (8 bits)
    uint8_t freq_lsb = fnum & 0xFF;
    ym2163_write_reg(slot, YM2163_REG_FREQ_LOW_CH0 + channel, freq_lsb);

    // Write frequency MSB + octave + key state
    // freq_msb: bits 0-4 (5 bits from fnum >> 8)
    // octave: bits 5-6 (2 bits, octave << 5)
    // KON: bit 6 (0x40) overlaps with octave bit 1
    uint8_t freq_msb = (fnum >> 8) & 0x1F;  // 5 bits MSB
    uint8_t octave_bits = (octave & 0x03) << 5;  // 2 bits for octave (0-3)
    uint8_t freq_high = octave_bits | freq_msb;

    if (channel_state[slot][channel].key_on) {
        freq_high |= YM2163_KEY_ON_BIT;  // bit 6
    }

    ym2163_write_reg(slot, YM2163_REG_FREQ_HIGH_CH0 + channel, freq_high);
}

// Set timbre (waveform/envelope) for a channel
void ym2163_set_timbre(uint8_t slot, uint8_t channel, uint8_t timbre) {
    if (channel >= YM2163_NUM_CHANNELS) return;
    ym2163_write_reg(slot, YM2163_REG_TIMBRE_CH0 + channel, timbre);
}

// Set volume for a channel
void ym2163_set_volume(uint8_t slot, uint8_t channel, uint8_t volume) {
    if (channel >= YM2163_NUM_CHANNELS) return;
    ym2163_write_reg(slot, YM2163_REG_VOLUME_CH0 + channel, volume);
}

// Trigger a note on (following MARS21 project sequence)
void ym2163_note_on(uint8_t slot, uint8_t channel, uint8_t note, uint8_t velocity, uint8_t timbre) {
    if (channel >= YM2163_NUM_CHANNELS) return;
    if (note >= 128) return;

    // Get frequency data from table (default to middle C if out of range)
    uint16_t fnum_data = FNUM_TABLE[60]; // Default to middle C
    if (note >= 35 && note <= 85 && FNUM_TABLE[note] != 0) {
        fnum_data = FNUM_TABLE[note];
    }

    // Extract octave and F-NUM
    // Format from Z-80 code: bits [15:13] = octave, bits [12:0] = F-NUM
    uint8_t octave = (fnum_data >> 13) & 0x03;
    uint16_t fnum = fnum_data & 0x1FFF;

    // 1. Set timbre
    ym2163_set_timbre(slot, channel, timbre);

    // 2. Set volume from velocity
    // YM2163 volume register (8CH~8FH):
    //   Bits 5-4: VL2,VL1 (00=0dB, 01=-6dB, 10=-12dB, 11=-∞dB/mute)
    //   Bits 3-0: F3,F2,F1 output routing
    // Map MIDI velocity (0-127) to VL (0-2, avoid VL=3 which is mute)
    uint8_t vl;
    if (velocity >= 96) {
        vl = 0;  // 0 dB (max volume)
    } else if (velocity >= 64) {
        vl = 1;  // -6 dB
    } else {
        vl = 2;  // -12 dB
    }
    // Construct volume value: VL in bits 5-4, output routing in bits 3-0
    uint8_t volume_data = (vl << 4) | 0x07;  // 0x07 = OR1/OR2/OR3 outputs (MIA42 standard)
    ym2163_set_volume(slot, channel, volume_data);

    // 3. Set pitch with key OFF
    channel_state[slot][channel].key_on = 0;
    ym2163_set_frequency(slot, channel, fnum, octave);

    spfm_flush();

    // 4. Key ON
    // freq_msb = bits 0-4 (5 bits), octave = bits 5-6 (2 bits), KON = bit 6
    channel_state[slot][channel].key_on = 1;
    uint8_t freq_msb = (fnum >> 8) & 0x1F;
    uint8_t octave_bits = (octave & 0x03) << 5;
    ym2163_write_reg(slot, YM2163_REG_FREQ_HIGH_CH0 + channel,
                     YM2163_KEY_ON_BIT | octave_bits | freq_msb);

    spfm_flush();
}

// Trigger a note off
void ym2163_note_off(uint8_t slot, uint8_t channel) {
    if (channel >= YM2163_NUM_CHANNELS) return;

    channel_state[slot][channel].key_on = 0;

    // Clear key-on bit while preserving frequency
    // freq_msb = bits 0-4 (5 bits), octave = bits 5-6 (2 bits), KON = 0
    uint8_t freq_msb = (channel_state[slot][channel].fnum >> 8) & 0x1F;
    uint8_t octave_bits = (channel_state[slot][channel].octave & 0x03) << 5;
    ym2163_write_reg(slot, YM2163_REG_FREQ_HIGH_CH0 + channel,
                     octave_bits | freq_msb);

    spfm_flush();
}
