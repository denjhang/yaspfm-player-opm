#include "ssg_to_opm.h"
#include "spfm.h"
#include "chiptype.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// --- Internal State ---
static uint8_t _regs[16];
static double _clock_ratio;
static uint32_t _source_clock;
static double _ssg_clock;
static chip_type_t _source_chip;
static opm_write_func_t _write_func;
static ay_stereo_mode_t _current_stereo_mode = AY_STEREO_ABC;
static bool _noise_channel_is_on = false;

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

// --- Constants ---
static const int VOL_TO_TL[] = {127, 62, 56, 52, 46, 42, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0};
static const int N_VOL_TO_TL[] = {127, 126, 125, 124, 123, 122, 121, 120, 116, 112, 105, 96, 82, 64, 37, 0};

// --- YM2608 Channel Management (Static Mapping) ---
static int ssg_to_opm_map[3] = {-1, -1, -1}; // Maps SSG channel (0-2) to an OPM channel, or -1 if not mapped

// Channel mapping for YM2203
static int OPM_CH_BASE_2203 = 3; // FM uses 0, 1, 2. SSG Tones use 3, 4, 5
static int OPM_NOISE_CH_2203 = 7; // SSG Noise uses the last channel

// Stereo Panning Constants
#define OPM_PAN_LEFT  0x40
#define OPM_PAN_RIGHT 0x80
#define OPM_PAN_CENTER 0xC0

static const char* STEREO_MODE_NAMES[] = {"ABC", "ACB", "BAC", "Mono"};

// --- Helper Functions ---
static void _y(uint8_t addr, uint8_t data) {
    if (_write_func) {
        _write_func(addr, data);
    }
}

void ssg_to_opm_set_fixed_mapping(const int* ssg_channels, const int* opm_channels, int count) {
    // Reset existing mapping
    for (int i = 0; i < 3; i++) {
        ssg_to_opm_map[i] = -1;
    }
    // Apply new mapping
    for (int i = 0; i < count; i++) {
        if (ssg_channels[i] >= 0 && ssg_channels[i] < 3) {
            ssg_to_opm_map[ssg_channels[i]] = opm_channels[i];
        }
    }
}

static int toOpmCh(int ssgCh) {
    if (ssgCh < 0 || ssgCh > 2) return -1;

    if (_source_chip == CHIP_TYPE_YM2608) {
        return ssg_to_opm_map[ssgCh]; // Return pre-calculated fixed mapping
    }
    
    // For YM2203, use direct mapping as channels are sufficient
    return ssgCh + OPM_CH_BASE_2203;
}

static void freqToOPMNote(double freq, double clockRatio, uint8_t* kc, uint8_t* kf) {
    const double BASE_FREQ_OPM = 277.2;
    const int KEY_TO_NOTE_OPM[] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14};

    if (freq <= 0 || isinf(freq)) {
        *kc = 0; *kf = 0;
        return;
    }
    double key = fmax(0.0, 60.0 + log2((freq * clockRatio) / BASE_FREQ_OPM) * 12.0);
    double frac = key - floor(key);
    *kf = (uint8_t)floor(frac * 64.0);
    int note = KEY_TO_NOTE_OPM[(int)floor(key) % 12];
    int oct = (int)fmax(0, floor(key / 12.0) - 2); // Lower by 2 octaves
    *kc = (oct << 4) | note;
}

static void _updateFreq(int ch, double freq) {
    uint8_t kc, kf;
    freqToOPMNote(freq, _clock_ratio, &kc, &kf);
    int opmCh = toOpmCh(ch);
    if (opmCh == -1 || opmCh >= 8) return; // Channel not allocated or out of bounds
    _y(0x28 + opmCh, kc);
    _y(0x30 + opmCh, kf << 2);
}

static void _updateNoise() {
    int nfreq = _regs[6] & 0x1f;
    _y(0x0f, 0x80 | (0x1f - nfreq)); // Set global noise frequency

    if (_source_chip == CHIP_TYPE_YM2608) {
        // In YM2608 mode, noise is mixed in software by prioritizing channels.
        // No dedicated hardware noise channel is used.
        return;
    }

    // YM2203 uses a dedicated noise channel
    int noise_opm_ch = OPM_NOISE_CH_2203;
    if (noise_opm_ch >= 8) return;

    int nVol = 0;
    bool is_any_noise_on = false;
    bool noise_on_left = false;
    bool noise_on_right = false;

    uint8_t ch_pan_map[3];
    switch (_current_stereo_mode) {
        case AY_STEREO_ABC: ch_pan_map[0] = OPM_PAN_LEFT; ch_pan_map[1] = OPM_PAN_CENTER; ch_pan_map[2] = OPM_PAN_RIGHT; break;
        case AY_STEREO_ACB: ch_pan_map[0] = OPM_PAN_LEFT; ch_pan_map[2] = OPM_PAN_CENTER; ch_pan_map[1] = OPM_PAN_RIGHT; break;
        case AY_STEREO_BAC: ch_pan_map[1] = OPM_PAN_LEFT; ch_pan_map[0] = OPM_PAN_CENTER; ch_pan_map[2] = OPM_PAN_RIGHT; break;
        default: ch_pan_map[0] = OPM_PAN_CENTER; ch_pan_map[1] = OPM_PAN_CENTER; ch_pan_map[2] = OPM_PAN_CENTER; break;
    }

    for (int i = 0; i < 3; i++) {
        if ((_regs[7] & (0x8 << i)) == 0) { // if noise is enabled for channel i
            is_any_noise_on = true;
            nVol = fmax(nVol, _regs[8 + i] & 0xf);
            uint8_t pan = ch_pan_map[i];
            if (pan == OPM_PAN_LEFT || pan == OPM_PAN_CENTER) noise_on_left = true;
            if (pan == OPM_PAN_RIGHT || pan == OPM_PAN_CENTER) noise_on_right = true;
        }
    }

    uint8_t final_noise_pan = 0;
    if (noise_on_left && noise_on_right) final_noise_pan = OPM_PAN_CENTER;
    else if (noise_on_left) final_noise_pan = OPM_PAN_LEFT;
    else if (noise_on_right) final_noise_pan = OPM_PAN_RIGHT;
    else final_noise_pan = OPM_PAN_CENTER;

    _y(0x20 + noise_opm_ch, (final_noise_pan & 0xC0) | 0x3F); // Set pan, use noise voice

    int tl = N_VOL_TO_TL[nVol];
    if (is_any_noise_on) {
        if (!_noise_channel_is_on) {
            _y(0x08, (0xF << 3) | noise_opm_ch); // Key On
            _noise_channel_is_on = true;
        }
        // tl -= 13; // Removed volume amplification as per user feedback.
    } else {
        if (_noise_channel_is_on) {
            _y(0x08, noise_opm_ch); // Key Off
            _noise_channel_is_on = false;
        }
    }
    _y(0x78 + noise_opm_ch, fmax(0, fmin(127, tl)));
}

static void _updateTone(int ch) {
    int opmCh = toOpmCh(ch);
    if (opmCh == -1 || opmCh >= 8) return;

    const int v = _regs[8 + ch];
    const bool tone_on = (_regs[7] & (1 << ch)) == 0;
    const bool noise_on = (_regs[7] & (8 << ch)) == 0;
    const bool use_envelope = (v & 0x10) != 0;

    // In SSG, if noise is enabled on a channel, the tone is muted.
    // This behavior should be replicated for YM2203.
    // For YM2608, we don't have a dedicated noise channel, so we play the tone regardless,
    // and rely on the pre-scan to prioritize the noise-enabled channel.
    bool tone_muted_by_noise = (_source_chip == CHIP_TYPE_YM2203) && noise_on;

    int final_vol = 0;
    if (use_envelope) {
        final_vol = _envelope_value >> 1;
    } else {
        final_vol = v & 0xf;
    }

    // If tone is off OR muted by noise (on YM2203), set volume to max attenuation
    if (!tone_on || tone_muted_by_noise) {
        _y(0x70 + opmCh, 0x7f); // M1 Total Level
    } else {
        int tl = VOL_TO_TL[final_vol & 0xf];
        // Reduce volume to ~0.9x by increasing attenuation. 3 TL is approx -1dB.
        tl += 3; 
        _y(0x70 + opmCh, fmin(127, tl));
    }
}

static void _reset_envelope_segment() {
    envelope_proc_t proc = ENVELOPE_SHAPES[_envelope_shape][_envelope_segment];
    if (proc == CSlideDown || proc == CHoldTop) _envelope_value = 31;
    else _envelope_value = 0;
}

void ssg_to_opm_update_envelope(void) {
    _envelope_counter++;
    if (_envelope_counter >= _envelope_period) {
        _envelope_counter = 0;
        switch (ENVELOPE_SHAPES[_envelope_shape][_envelope_segment]) {
            case CSlideUp:
                _envelope_value++;
                if (_envelope_value > 31) { _envelope_segment ^= 1; _reset_envelope_segment(); }
                break;
            case CSlideDown:
                _envelope_value--;
                if (_envelope_value < 0) { _envelope_segment ^= 1; _reset_envelope_segment(); }
                break;
            case CHoldTop: case CHoldBottom: break;
        }
        for (int i = 0; i < 3; i++) {
            if (_regs[8 + i] & 0x10) _updateTone(i);
        }
    }
}

void ssg_to_opm_init(chip_type_t source_chip_type, uint32_t source_clock, opm_write_func_t write_func) {
    _source_chip = source_chip_type;
    _source_clock = source_clock;
    _write_func = write_func;

    // The SSG part of YM2203/YM2608 runs at CLOCK/2.
    _ssg_clock = (double)source_clock / 2.0;

    const double OPM_CLOCK = get_chip_default_clock(CHIP_TYPE_YM2151);
    _clock_ratio = _ssg_clock / OPM_CLOCK;

    memset(_regs, 0, sizeof(_regs));

    // Reset mapping for YM2608. It will be set by vgm_analyze_and_set_ssg_mapping.
    if (_source_chip == CHIP_TYPE_YM2608) {
        for(int i=0; i<3; i++) {
            ssg_to_opm_map[i] = -1;
        }
    }

    ssg_to_opm_set_stereo_mode(_current_stereo_mode);

    // Initialize a simple square wave voice for all potential SSG channels
    int max_ch = (_source_chip == CHIP_TYPE_YM2608) ? 8 : 6;
    for (int i = 0; i < max_ch; i++) {
        _y(0x40 + i, 0x02); // DT1/MUL
        _y(0x50 + i, 0x01); // DT1/MUL for op 2
        _y(0x60 + i, 0x1b); // TL
        _y(0x70 + i, 0x7f); // TL for op 2
        _y(0x80 + i, 0x1f); // AR/KSR
        _y(0x90 + i, 0x1f); // AR/KSR for op 2
        _y(0xa0 + i, 0);    // D1R/AM
        _y(0xb0 + i, 0);    // D1R/AM for op 2
        _y(0xc0 + i, 0);    // D2R
        _y(0xd0 + i, 0);    // D2R for op 2
        _y(0xe0 + i, 0);    // D1L/RR
        _y(0xf0 + i, 0);    // D1L/RR for op 2
    }

    // Initialize dedicated noise channel for YM2203
    if (_source_chip == CHIP_TYPE_YM2203) {
        int noise_ch = OPM_NOISE_CH_2203;
        _y(0x20 + noise_ch, 0xC0 | 0x3F); // Center Pan, use noise voice
        _y(0x58 + noise_ch, 0x00); // DT1/MUL
        _y(0x78 + noise_ch, 0x7f); // TL
        _y(0x98 + noise_ch, 0x1f); // AR/KSR
        _y(0xb8 + noise_ch, 0);    // D1R/AM
        _y(0xd8 + noise_ch, 0);    // D2R
        _y(0xf8 + noise_ch, 0);    // D1L/RR
        _noise_channel_is_on = false;
    }
}

static void _recalculate_freq(int ch) {
    const int v = _regs[8 + ch];
    if ((v & 0x10) && _envelope_period < 200) {
        int steps = 0;
        switch (_envelope_shape) {
            case 8: case 9: case 11: case 12: case 13: case 15: steps = 32; break;
            case 10: case 14: steps = 64; break;
            default: steps = 0; break;
        }
        if (steps > 0) {
            const double freq = _ssg_clock / (16.0 * _envelope_period * steps);
            _updateFreq(ch, freq);
            return;
        }
    }

    const int tp = ((_regs[ch * 2 + 1] & 0x0F) << 8) | _regs[ch * 2];
    if (tp == 0) {
        _updateFreq(ch, 0);
    } else {
        // The effective clock for SSG tone is CLOCK / 2, and the period divisor is 16.
        // AY-3-8910 formula is MasterClock / (16 * TP).
        // YM2203/2608 SSG is similar.
        // The user reported pitch is high, and requested a fixed divisor of 4.5.
        // This implies the clock calculation needs adjustment.
        // Let's adjust the clock or the divisor.
        // _ssg_clock is already source_clock / 2.
        // Let's try to adjust the divisor from 16.0 to something else.
        // A divisor of 4.5 seems arbitrary and might be a misinterpretation.
        // Let's re-examine the clock. The SSG clock is CLOCK/2. The OPM clock is 3.579545MHz.
        // The issue might be in the clock ratio.
        // Let's apply a fixed divisor as requested.
        double divisor = 4.5;
        if (_source_chip == CHIP_TYPE_YM2608) {
            divisor = 18.0; // Quadruple the divisor for YM2608 to lower by two octaves
        }
        const double freq = _ssg_clock / (divisor * tp);
        _updateFreq(ch, freq);
    }
}

void ssg_to_opm_write_reg(uint8_t addr, uint8_t data) {
    if (addr > 15) return;
    uint8_t old_data = _regs[addr];
    _regs[addr] = data;

    if (addr <= 5) { // Tone period
        int ch = addr >> 1;
        _recalculate_freq(ch);
        _updateTone(ch);
        if ((_regs[8 + ch] & 0x10)) {
            if (!(_envelope_shape >= 8 && _envelope_shape <= 15 && _envelope_shape != 9 && _envelope_shape != 11)) {
                _envelope_counter = 0; _envelope_segment = 0; _reset_envelope_segment();
            }
        }
    } else if (addr >= 8 && addr <= 10) { // Volume
        int ch = addr - 8;
        int opmCh = toOpmCh(ch);
        if (opmCh != -1 && opmCh < 8) {
            bool old_env = (old_data & 0x10) != 0;
            bool new_env = (data & 0x10) != 0;
            uint8_t old_vol = old_data & 0x0F;
            uint8_t new_vol = data & 0x0F;
            bool tone_on = (_regs[7] & (1 << ch)) == 0;

            if (tone_on) {
                if (!old_env && old_vol == 0 && (new_vol > 0 || new_env)) { // Key On
                    _recalculate_freq(ch);
                     _y(0x08, (0xF << 3) | opmCh);
                } else if (!new_env && new_vol == 0 && (old_vol > 0 || old_env)) { // Key Off
                    _y(0x08, opmCh);
                }
            }
        }
        _updateTone(ch);
        _recalculate_freq(ch);
        _updateNoise();
    } else if (addr == 6) { // Noise period
        _updateNoise();
    } else if (addr == 7) { // Mixer
        if (_source_chip == CHIP_TYPE_YM2203) {
            for (int i = 0; i < 3; i++) {
                bool old_tone_on = (old_data & (1 << i)) == 0;
                bool new_tone_on = (data & (1 << i)) == 0;
                int opmCh = toOpmCh(i);
                if (opmCh == -1 || opmCh >= 8) continue;

                if (new_tone_on && !old_tone_on) { // Key On
                    _recalculate_freq(i);
                    _updateTone(i);
                    ssg_to_opm_set_stereo_mode(_current_stereo_mode);
                    _y(0x08, (0xF << 3) | opmCh); // Key On for all slots of the channel
                    if ((_regs[8 + i] & 0x10)) {
                        if (!(_envelope_shape >= 8 && _envelope_shape <= 15 && _envelope_shape != 9 && _envelope_shape != 11)) {
                            _envelope_counter = 0; 
                            _envelope_segment = 0; 
                            _reset_envelope_segment();
                        }
                    }
                } else if (!new_tone_on && old_tone_on) { // Key Off
                    _y(0x08, opmCh);
                }
            }
        } else { // YM2608 logic remains the same
            for (int i = 0; i < 3; i++) {
                bool old_tone_on = (old_data & (1 << i)) == 0;
                bool new_tone_on = (data & (1 << i)) == 0;
                int opmCh = toOpmCh(i);

                if (new_tone_on && !old_tone_on) { // Key On
                    if (opmCh == -1 || opmCh >= 8) continue;

                    _recalculate_freq(i);
                    _updateTone(i);
                    ssg_to_opm_set_stereo_mode(_current_stereo_mode); // Re-apply stereo on key-on
                    _y(0x08, (0xf << 3) | opmCh);
                    if ((_regs[8 + i] & 0x10)) {
                        if (!(_envelope_shape >= 8 && _envelope_shape <= 15 && _envelope_shape != 9 && _envelope_shape != 11)) {
                            _envelope_counter = 0; 
                            _envelope_segment = 0; 
                            _reset_envelope_segment();
                        }
                    }
                } else if (!new_tone_on && old_tone_on) { // Key Off
                    if (opmCh == -1 || opmCh >= 8) continue;
                    _y(0x08, opmCh);
                }
            }
        }
        // Update tones and noise for any non-key-on related mixer changes (e.g. noise enable/disable)
        _updateTone(0); 
        _updateTone(1); 
        _updateTone(2);
        _updateNoise();
    } else if (addr == 11 || addr == 12) {
        _envelope_period = (_regs[12] << 8) | _regs[11];
        if (_envelope_period == 0) _envelope_period = 1;
        _recalculate_freq(0); _recalculate_freq(1); _recalculate_freq(2);
    } else if (addr == 13) {
        _envelope_shape = data & 0x0f;
        _envelope_counter = 0; _envelope_segment = 0; _reset_envelope_segment();
        _recalculate_freq(0); _recalculate_freq(1); _recalculate_freq(2);
    }
}

const char* ssg_to_opm_get_stereo_mode_name(ay_stereo_mode_t mode) {
    if (mode >= 0 && mode < AY_STEREO_MODE_COUNT) return STEREO_MODE_NAMES[mode];
    return "Invalid";
}

ay_stereo_mode_t ssg_to_opm_get_current_stereo_mode(void) {
    return _current_stereo_mode;
}

ay_stereo_mode_t ssg_to_opm_cycle_stereo_mode(void) {
    ay_stereo_mode_t next_mode = (_current_stereo_mode + 1) % AY_STEREO_MODE_COUNT;
    ssg_to_opm_set_stereo_mode(next_mode);
    return next_mode;
}

void ssg_to_opm_set_stereo_mode(ay_stereo_mode_t mode) {
    _current_stereo_mode = mode;
    if (!_write_func) return;

    uint8_t ch_pan_map[3];
    switch (mode) {
        case AY_STEREO_ABC: ch_pan_map[0] = OPM_PAN_LEFT; ch_pan_map[1] = OPM_PAN_CENTER; ch_pan_map[2] = OPM_PAN_RIGHT; break;
        case AY_STEREO_ACB: ch_pan_map[0] = OPM_PAN_LEFT; ch_pan_map[2] = OPM_PAN_CENTER; ch_pan_map[1] = OPM_PAN_RIGHT; break;
        case AY_STEREO_BAC: ch_pan_map[1] = OPM_PAN_LEFT; ch_pan_map[0] = OPM_PAN_CENTER; ch_pan_map[2] = OPM_PAN_RIGHT; break;
        default: ch_pan_map[0] = OPM_PAN_CENTER; ch_pan_map[1] = OPM_PAN_CENTER; ch_pan_map[2] = OPM_PAN_CENTER; break;
    }

    for (int i = 0; i < 3; i++) {
        int opmCh = toOpmCh(i);
        if (opmCh != -1 && opmCh < 8) {
            _y(0x20 + opmCh, (ch_pan_map[i] & 0xC0) | 0x3C);
        }
    }
    _updateNoise();
}
