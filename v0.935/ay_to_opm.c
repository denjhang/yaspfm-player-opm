#include "ay_to_opm.h"
#include "spfm.h"
#include "chiptype.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// --- Internal State ---
static uint8_t _regs[16];
static double _clock_ratio;
static uint32_t _source_clock;
static chip_type_t _source_chip;
static uint8_t _opm_slot;
static int _fdiv;
static opm_write_func_t _write_func;
static ay_stereo_mode_t _current_stereo_mode = AY_STEREO_ABC; // Default stereo mode

// Envelope state
static int _envelope_counter = 0;
static int _envelope_period = 1;
static int _envelope_shape = 0;
static int _envelope_segment = 0;
static int _envelope_value = 0;

typedef enum {
    CSlideUp,
    CSlideDown,
    CHoldBottom,
    CHoldTop
} envelope_proc_t;

static const envelope_proc_t ENVELOPE_SHAPES[16][2] = {
    {CSlideDown, CHoldBottom}, {CSlideDown, CHoldBottom}, {CSlideDown, CHoldBottom}, {CSlideDown, CHoldBottom},
    {CSlideUp, CHoldBottom},   {CSlideUp, CHoldBottom},   {CSlideUp, CHoldBottom},   {CSlideUp, CHoldBottom},
    {CSlideDown, CSlideDown},  {CSlideDown, CHoldBottom}, {CSlideDown, CSlideUp},    {CSlideDown, CHoldTop},
    {CSlideUp, CSlideUp},      {CSlideUp, CHoldTop},      {CSlideUp, CSlideDown},    {CSlideUp, CHoldBottom}
};


// --- Constants from vgm-conv-main ---
static const int VOL_TO_TL[] = {127, 62, 56, 52, 46, 42, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0};
static const int N_VOL_TO_TL[] = {127, 126, 125, 124, 123, 122, 121, 120, 116, 112, 105, 96, 82, 64, 37, 0};
static const int OPM_CH_BASE = 4; // Use channels 4, 5, 6 for PSG tones, 7 for noise

// Stereo Panning Constants
#define OPM_PAN_LEFT  0x40 // C2
#define OPM_PAN_RIGHT 0x80 // C1
#define OPM_PAN_CENTER 0xC0 // C1 & C2

static const char* STEREO_MODE_NAMES[] = {
    "ABC", "ACB", "BAC", "Mono"
};

// --- Helper Functions ---
static void _y(uint8_t addr, uint8_t data) {
    if (_write_func) {
        _write_func(addr, data);
    }
}

static int toOpmCh(int psgCh) {
    return psgCh + OPM_CH_BASE;
}

// Re-implement freqToOPMNote from opm_freq.ts logic
static void freqToOPMNote(double freq, double clockRatio, uint8_t* kc, uint8_t* kf) {
    const double BASE_FREQ_OPM = 277.2; // C#4 = 60
    const int KEY_TO_NOTE_OPM[] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14};

    if (freq <= 0 || isinf(freq)) {
        *kc = 0;
        *kf = 0;
        return;
    }
    double key = fmax(0.0, 60.0 + log2((freq * clockRatio) / BASE_FREQ_OPM) * 12.0);
    double frac = key - floor(key);
    *kf = (uint8_t)floor(frac * 64.0);
    int note = KEY_TO_NOTE_OPM[(int)floor(key) % 12];
    int oct = (int)fmin(7.0, floor(key / 12.0));
    *kc = (oct << 4) | note;
}

static void _updateFreq(int ch, double freq) {
    uint8_t kc, kf;
    freqToOPMNote(freq, _clock_ratio, &kc, &kf);
    int opmCh = toOpmCh(ch);
    _y(0x28 + opmCh, kc);
    _y(0x30 + opmCh, kf << 2);
}

static void _updateNoise() {
    int nVol = 0;
    bool noise_on_left = false;
    bool noise_on_right = false;

    uint8_t ch_pan_map[3]; // Pan for AY channels A, B, C
    // This logic duplicates the start of ay_to_opm_set_stereo_mode to get the current pan map
    switch (_current_stereo_mode) {
        case AY_STEREO_ABC: ch_pan_map[0] = OPM_PAN_LEFT; ch_pan_map[1] = OPM_PAN_CENTER; ch_pan_map[2] = OPM_PAN_RIGHT; break;
        case AY_STEREO_ACB: ch_pan_map[0] = OPM_PAN_LEFT; ch_pan_map[2] = OPM_PAN_CENTER; ch_pan_map[1] = OPM_PAN_RIGHT; break;
        case AY_STEREO_BAC: ch_pan_map[1] = OPM_PAN_LEFT; ch_pan_map[0] = OPM_PAN_CENTER; ch_pan_map[2] = OPM_PAN_RIGHT; break;
        default: /* MONO */ ch_pan_map[0] = OPM_PAN_CENTER; ch_pan_map[1] = OPM_PAN_CENTER; ch_pan_map[2] = OPM_PAN_CENTER; break;
    }

    for (int i = 0; i < 3; i++) {
        // Check if noise is enabled for this AY channel
        if ((_regs[7] & (0x8 << i)) == 0) {
            // Aggregate the max volume from all enabled channels
            nVol = fmax(nVol, _regs[8 + i] & 0xf);
            
            // Determine the stereo position based on the channel's panning
            uint8_t pan = ch_pan_map[i];
            if (pan == OPM_PAN_LEFT || pan == OPM_PAN_CENTER) {
                noise_on_left = true;
            }
            if (pan == OPM_PAN_RIGHT || pan == OPM_PAN_CENTER) {
                noise_on_right = true;
            }
        }
    }

    // Determine final noise panning
    uint8_t final_noise_pan = 0;
    if (noise_on_left && noise_on_right) {
        final_noise_pan = OPM_PAN_CENTER;
    } else if (noise_on_left) {
        final_noise_pan = OPM_PAN_LEFT;
    } else if (noise_on_right) {
        final_noise_pan = OPM_PAN_RIGHT;
    } else {
        // If no channels have noise, volume will be 0, effectively muting it.
        // We can set pan to center as a default.
        final_noise_pan = OPM_PAN_CENTER;
    }

    int nfreq = _regs[6] & 0x1f;
    const int opmNoiseCh = 7;
    _y(0x0f, 0x80 | (0x1f - nfreq)); // Set noise frequency for OPM
    _y(0x20 + opmNoiseCh, (final_noise_pan & 0xC0) | 0x3C); // Set noise channel panning
    _y(0x78 + opmNoiseCh, fmin(127, N_VOL_TO_TL[nVol])); // Set noise volume on C2 of channel 8
}

static void _updateTone(int ch) {
    const int v = _regs[8 + ch];
    const int tone_enabled = ((1 << ch) & _regs[7]) == 0;
    const int envelope_as_waveform = (v & 0x10) && (_envelope_period < 200);

    int opmCh = toOpmCh(ch);

    if (tone_enabled || envelope_as_waveform) {
        int tVol;
        if (v & 0x10) { // Envelope mode
            if (envelope_as_waveform) {
                // When envelope is a waveform, volume is max.
                tVol = 15; 
            } else {
                // When envelope is just for volume, use its current value.
                tVol = _envelope_value >> 1;
            }
        } else {
            // Fixed volume.
            tVol = v & 0xf;
        }
        _y(0x70 + opmCh, fmin(127, VOL_TO_TL[tVol & 0xf]));
    } else {
        // Mute if neither tone is enabled nor envelope is used as a waveform.
        _y(0x70 + opmCh, 0x7f);
    }
}

static void _reset_envelope_segment() {
    envelope_proc_t proc = ENVELOPE_SHAPES[_envelope_shape][_envelope_segment];
    if (proc == CSlideDown || proc == CHoldTop) {
        _envelope_value = 31;
    } else {
        _envelope_value = 0;
    }
}

void ay_to_opm_update_envelope(void) {
    _envelope_counter++;
    if (_envelope_counter >= _envelope_period) {
        _envelope_counter = 0;
        switch (ENVELOPE_SHAPES[_envelope_shape][_envelope_segment]) {
            case CSlideUp:
                _envelope_value++;
                if (_envelope_value > 31) {
                    _envelope_segment ^= 1;
                    _reset_envelope_segment();
                }
                break;
            case CSlideDown:
                _envelope_value--;
                if (_envelope_value < 0) {
                    _envelope_segment ^= 1;
                    _reset_envelope_segment();
                }
                break;
            case CHoldTop:
            case CHoldBottom:
                // Do nothing
                break;
        }
        // Update tone volumes if they are in envelope mode
        for (int i = 0; i < 3; i++) {
            if (_regs[8 + i] & 0x10) {
                _updateTone(i);
            }
        }
    }
}


// --- Public API ---

void ay_to_opm_init(chip_type_t source_chip_type, uint32_t source_clock, opm_write_func_t write_func) {
    _source_chip = source_chip_type;
    _source_clock = source_clock;
    _opm_slot = get_slot_for_chip(CHIP_TYPE_YM2151); // Still useful for direct hardware access if needed
    _write_func = write_func;

    const double OPM_CLOCK = get_chip_default_clock(CHIP_TYPE_YM2151);
    _clock_ratio = (double)source_clock / OPM_CLOCK;
    // _fdiv is no longer used in frequency calculation, but we keep it for historical context or future use.
    _fdiv = (_source_chip == CHIP_TYPE_AY8910) ? 2 : 4;

    memset(_regs, 0, sizeof(_regs));

    // --- Initial Commands ---
    // Set initial panning and other channel params
    ay_to_opm_set_stereo_mode(_current_stereo_mode);

    // PSG TONE Channels (4, 5, 6)
    for (int i = 0; i < 3; i++) {
        int opmCh = toOpmCh(i);
        // Panning is now set by ay_to_opm_set_stereo_mode, which is called right before this loop.
        // The following writes initialize the voice properties.
        _y(0x40 + opmCh, 0x02); // M1: DT=0 ML=2
        _y(0x50 + opmCh, 0x01); // C1: DT=0 ML=1
        _y(0x60 + opmCh, 0x1b); // M1: TL=27
        _y(0x70 + opmCh, 0x7f); // C1: TL=127 (mute)
        _y(0x80 + opmCh, 0x1f); // M1: AR=31
        _y(0x90 + opmCh, 0x1f); // C1: AR=31
        _y(0xa0 + opmCh, 0);    // M1: DR=0
        _y(0xb0 + opmCh, 0);    // C1: DR=0
        _y(0xc0 + opmCh, 0);    // M1: DT2=0 SR=0
        _y(0xd0 + opmCh, 0);    // C1: DT2=0 SR=0
        _y(0xe0 + opmCh, 0);    // M1: SL=0 RR=0
        _y(0xf0 + opmCh, 0);    // C1: SL=0 RR=0
        _y(0x08, (0xf << 3) | opmCh); // KEY ON
    }

    // Noise Channel (7)
    {
        const int opmCh = 7;
        _y(0x20 + opmCh, 0xfc); // RL=ON FB=7 CON=4
        _y(0x58 + opmCh, 0x00); // C2: DT=0 ML=0
        _y(0x78 + opmCh, 0x7f); // C2: TL=127 (mute)
        _y(0x98 + opmCh, 0x1f); // C2: AR=31
        _y(0xb8 + opmCh, 0);    // C2: DR=0
        _y(0xd8 + opmCh, 0);    // C2: DT2=0 SR=0
        _y(0xf8 + opmCh, 0);    // C2: SL=0 RR=0
        _y(0x08, (0x8 << 3) | opmCh); // slot32 only KEY ON
    }
}

static void _recalculate_freq(int ch) {
    const int v = _regs[8 + ch];
    // Check if channel is in envelope mode and envelope period is short
    if ((v & 0x10) && _envelope_period < 200) {
        // Shapes 8, 9, 11, 12, 13, 15 are single-cycle (32 steps)
        // Shapes 10, 14 are dual-cycle (64 steps)
        // Other shapes are one-shot, don't treat as waveform
        int steps = 0;
        switch (_envelope_shape) {
            case 8: case 11: case 12: case 13: // Sawtooth waves
                steps = 32;
                break;
            case 10: case 14: // Triangle waves
                steps = 64;
                break;
            // Shapes 9 and 15 are also repeating, but behave differently.
            // Shape 9: Saw down, hold bottom
            // Shape 15: Saw up, hold bottom
            // Let's treat them as 32-step for now.
            case 9: case 15:
                steps = 32;
                break;
            default:
                steps = 0; 
                break;
        }

        if (steps > 0) {
            const double freq = (double)_source_clock / (16.0 * _envelope_period * steps);
            _updateFreq(ch, freq);
            return; // Use envelope frequency
        }
    }

    // Default to tone period frequency
    const int tp = ((_regs[ch * 2 + 1] & 0x0F) << 8) | _regs[ch * 2];
    if (tp == 0) {
        _updateFreq(ch, 0);
    } else {
        const double freq = (double)_source_clock / (16.0 * tp);
        _updateFreq(ch, freq);
    }
}

void ay_to_opm_write_reg(uint8_t addr, uint8_t data) {
    if (addr > 15) return;

    uint8_t old_data = _regs[addr];
    _regs[addr] = data;

    if (addr <= 5) { // Tone period
        int ch = addr >> 1;
        _recalculate_freq(ch);
        _updateTone(ch); // FIX: Ensure volume is updated on frequency change to prevent dropped notes in fast arpeggios.

        // FIX: Re-trigger one-shot envelopes on note change to prevent fade-out on fast arpeggios.
        if ((_regs[8 + ch] & 0x10)) { // If channel uses envelope
            // Repeating shapes are 8, 10, 12, 14. All others are one-shot and need reset.
            if (!(_envelope_shape == 8 || _envelope_shape == 10 || _envelope_shape == 12 || _envelope_shape == 14)) {
                _envelope_counter = 0;
                _envelope_segment = 0;
                _reset_envelope_segment();
            }
        }
    } else if (addr >= 8 && addr <= 10) { // Amplitudes
        int ch = addr - 8;
        _updateTone(ch);
        _recalculate_freq(ch); // Recalculate in case envelope waveform status changed
        _updateNoise();
    } else if (addr == 6) { // Noise period
        _updateNoise();
    } else if (addr == 7) { // Mixer
        for (int i = 0; i < 3; i++) {
            bool old_tone_on = !((old_data >> i) & 1);
            bool new_tone_on = !((data >> i) & 1);
            int opmCh = toOpmCh(i);

            if (new_tone_on && !old_tone_on) { // Key On
                _recalculate_freq(i);
                _updateTone(i);
                _y(0x08, (0xf << 3) | opmCh); // KEY ON all slots

                // If this channel uses a one-shot envelope, reset it.
                if ((_regs[8 + i] & 0x10)) {
                    if (!(_envelope_shape == 8 || _envelope_shape == 10 || _envelope_shape == 12 || _envelope_shape == 14)) {
                        _envelope_counter = 0;
                        _envelope_segment = 0;
                        _reset_envelope_segment();
                    }
                }
            } else if (!new_tone_on && old_tone_on) { // Key Off
                 _y(0x08, opmCh); // KEY OFF all slots
            }
        }
        // Update tones and noise for any non-key-on related mixer changes (e.g. noise enable/disable)
        _updateTone(0);
        _updateTone(1);
        _updateTone(2);
        _updateNoise();
    } else if (addr == 11 || addr == 12) { // Envelope period
        _envelope_period = (_regs[12] << 8) | _regs[11];
        if (_envelope_period == 0) _envelope_period = 1;
        // Recalculate freq for all channels as envelope period affects them
        _recalculate_freq(0);
        _recalculate_freq(1);
        _recalculate_freq(2);
    } else if (addr == 13) { // Envelope shape
        _envelope_shape = data & 0x0f;
        _envelope_counter = 0;
        _envelope_segment = 0;
        _reset_envelope_segment();
        // Recalculate freq for all channels as envelope shape affects them
        _recalculate_freq(0);
        _recalculate_freq(1);
        _recalculate_freq(2);
    }
}

const char* ay_to_opm_get_stereo_mode_name(ay_stereo_mode_t mode) {
    if (mode >= 0 && mode < AY_STEREO_MODE_COUNT) {
        return STEREO_MODE_NAMES[mode];
    }
    return "Invalid";
}

void ay_to_opm_set_stereo_mode(ay_stereo_mode_t mode) {
    _current_stereo_mode = mode;
    if (!_write_func) return; // Don't do anything if not initialized

    uint8_t ch_pan[3]; // Pan for AY channels A, B, C

    switch (mode) {
        case AY_STEREO_ABC:
            ch_pan[0] = OPM_PAN_LEFT; ch_pan[1] = OPM_PAN_CENTER; ch_pan[2] = OPM_PAN_RIGHT;
            break;
        case AY_STEREO_ACB:
            ch_pan[0] = OPM_PAN_LEFT; ch_pan[2] = OPM_PAN_CENTER; ch_pan[1] = OPM_PAN_RIGHT;
            break;
        case AY_STEREO_BAC:
            ch_pan[1] = OPM_PAN_LEFT; ch_pan[0] = OPM_PAN_CENTER; ch_pan[2] = OPM_PAN_RIGHT;
            break;
        case AY_STEREO_MONO:
        default:
            ch_pan[0] = OPM_PAN_CENTER; ch_pan[1] = OPM_PAN_CENTER; ch_pan[2] = OPM_PAN_CENTER;
            break;
    }

    // Apply panning to OPM channels 4, 5, 6
    for (int i = 0; i < 3; i++) {
        int opmCh = toOpmCh(i);
        // RL bits are the top 2. FB=7, CON=4. So 0x3C is the base.
        _y(0x20 + opmCh, (ch_pan[i] & 0xC0) | 0x3C);
    }
    
    // Update noise panning as well, since it depends on the tone channels' panning
    _updateNoise();
}
