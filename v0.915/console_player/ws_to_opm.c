#include "ws_to_opm.h"
#include "spfm.h"
#include "chiptype.h"
#include "error.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NUM_WS_CHANNELS 4

// --- OPM Constants ---
#define OPM_PAN_LEFT  0x40
#define OPM_PAN_RIGHT 0x80
#define OPM_PAN_CENTER 0xC0

// --- Internal State ---
typedef struct {
    uint16_t period;
    uint8_t vol_left;
    uint8_t vol_right;
    bool enabled;
    bool active; // Note is currently playing
    uint32_t note_on_time; // in samples
    uint8_t last_opm_kc;
    uint8_t last_opm_kf;
} ws_channel_state_t;

static ws_channel_state_t _ch_state[NUM_WS_CHANNELS];
static uint8_t _regs[0x20]; // WS sound registers are from 0x80 to 0x9F
static uint32_t _total_samples = 0;
static double _clock_ratio;
static uint32_t _source_clock;
static chip_type_t _source_chip;
static opm_write_func_t _write_func;

// --- Helper Functions ---
static void _y(uint8_t addr, uint8_t data) {
    if (_write_func) {
        _write_func(addr, data);
    }
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
    // User feedback: Drop one octave (-12), raise 2.5 semitones (+2.5) -> net -9.5
    double key = fmax(0.0, 60.0 + log2((freq * clockRatio) / BASE_FREQ_OPM) * 12.0 - 9.5);
    double frac = key - floor(key);
    *kf = (uint8_t)floor(frac * 64.0);
    int note = KEY_TO_NOTE_OPM[(int)floor(key) % 12];
    int oct = (int)fmin(7.0, floor(key / 12.0));
    *kc = (oct << 4) | note;
}

static double period_to_freq(int period) {
    if (period >= 2048) return 0.0;
    return (3072000.0 / (2048.0 - period)) / 32.0;
}

static void _update_channel(int ch);

// --- Public API ---

void ws_to_opm_init(chip_type_t source_chip_type, uint32_t source_clock, opm_write_func_t write_func) {
    logging(LOG_LEVEL_DEBUG, "ws_to_opm_init called.");
    
    _source_chip = source_chip_type;
    _source_clock = source_clock;
    _write_func = write_func;

    const double OPM_CLOCK = get_chip_default_clock(CHIP_TYPE_YM2151);
    _clock_ratio = (double)source_clock / OPM_CLOCK;

    memset(_regs, 0, sizeof(_regs));
    memset(_ch_state, 0, sizeof(_ch_state));
    _total_samples = 0;

    // Initialize OPM channels 4,5,6,7 for WS tones
    for (int i = 0; i < NUM_WS_CHANNELS; i++) {
        _ch_state[i].enabled = true; // Assume channels are enabled by default
        int opmCh = i + 4;
        _y(0x20 + opmCh, OPM_PAN_CENTER | 0x3C); // Pan Center, FB=7, ALG=4
        _y(0x40 + opmCh, 0x02); // M1: DT=0 ML=2
        _y(0x50 + opmCh, 0x01); // C1: DT=0 ML=1
        _y(0x60 + opmCh, 0x1b); // M1: TL=27 (fixed modulator volume)
        _y(0x70 + opmCh, 0x7f); // C1: TL=127 (mute carrier initially)
        _y(0x80 + opmCh, 0x1f); // M1: AR=31
        _y(0x90 + opmCh, 0x1f); // C1: AR=31
        _y(0xa0 + opmCh, 0x00); // M1: DR=0
        _y(0xb0 + opmCh, 0x00); // C1: DR=0
        _y(0xc0 + opmCh, 0x00); // M1: DT2=0 SR=0
        _y(0xd0 + opmCh, 0x00); // C1: DT2=0 SR=0
        _y(0xe0 + opmCh, 0x0f); // M1: SL=0 RR=15
        _y(0xf0 + opmCh, 0x0f); // C1: SL=0 RR=15
    }

    // Initialize OPM channel 7 for noise, as per user feedback and ay_to_opm
    const int opmNoiseCh = 7;
    _y(0x20 + opmNoiseCh, OPM_PAN_CENTER | 0x3C); // Pan Center, FB=7, ALG=4
    _y(0x58 + opmNoiseCh, 0x00); // C2: DT=0 ML=0
    _y(0x78 + opmNoiseCh, 0x7f); // C2: TL=127 (mute)
    _y(0x98 + opmNoiseCh, 0x1f); // C2: AR=31
    _y(0xb8 + opmNoiseCh, 0x00); // C2: DR=0
    _y(0xd8 + opmNoiseCh, 0x00); // C2: DT2=0 SR=0
    _y(0xf8 + opmNoiseCh, 0x0f); // C2: SL=0 RR=15
    _y(0x08, (0x8 << 3) | opmNoiseCh); // Key On C2 slot
}

void ws_to_opm_write_reg(uint8_t port, uint8_t addr, uint8_t data) {
    (void)port; // The VGM command for WS is 0xBC.
    // For WS S-DSP, 'addr' is the register 0x00-0x1F.
    if (addr > 0x1F) return; // S-DSP registers are 0x00-0x1F

    _regs[addr] = data;

    uint16_t period;
    switch (addr) {
        case 0x00: case 0x01: // Ch1 Freq
            period = ((_regs[0x01] & 0x07) << 8) | _regs[0x00];
            _ch_state[0].period = (period == 0x7FF) ? 2048 : period;
            _update_channel(0);
            break;
        case 0x02: case 0x03: // Ch2 Freq
            period = ((_regs[0x03] & 0x07) << 8) | _regs[0x02];
            _ch_state[1].period = (period == 0x7FF) ? 2048 : period;
            _update_channel(1);
            break;
        case 0x04: case 0x05: // Ch3 Freq
            period = ((_regs[0x05] & 0x07) << 8) | _regs[0x04];
            _ch_state[2].period = (period == 0x7FF) ? 2048 : period;
            _update_channel(2);
            break;
        case 0x06: case 0x07: // Ch4 Freq
            period = ((_regs[0x07] & 0x07) << 8) | _regs[0x06];
            _ch_state[3].period = (period == 0x7FF) ? 2048 : period;
            _update_channel(3);
            break;

        case 0x08: _ch_state[0].vol_right = data & 0x0F; _ch_state[0].vol_left = data >> 4; _update_channel(0); break;
        case 0x09: _ch_state[1].vol_right = data & 0x0F; _ch_state[1].vol_left = data >> 4; _update_channel(1); break;
        case 0x0A: _ch_state[2].vol_right = data & 0x0F; _ch_state[2].vol_left = data >> 4; _update_channel(2); break;
        case 0x0B: _ch_state[3].vol_right = data & 0x0F; _ch_state[3].vol_left = data >> 4; _update_channel(3); break;

        case 0x0E: // Noise control - affects channel 3 if in noise mode
            _update_channel(3);
            break;
        
        case 0x0F: // Master Volume - not directly used, volume is per-channel
            break;

        case 0x10: // Channel Control (Enable bits, PCM/Noise mode)
            for (int i = 0; i < 4; ++i) {
                _ch_state[i].enabled = (data & (1 << i)) != 0;
            }
            // This register also controls noise mode for ch3, so update it.
            _update_channel(0);
            _update_channel(1);
            _update_channel(2);
            _update_channel(3);
            break;
    }
}

static void _update_channel(int ch) {
    ws_channel_state_t* state = &_ch_state[ch];
    
    // Determine if channel 3 is in noise mode.
    // This is the ONLY channel that can be noise.
    bool is_noise_mode = (ch == 3) && (_regs[0x10] & 0x80);

    bool should_be_on = state->enabled && (state->vol_left > 0 || state->vol_right > 0);

    if (is_noise_mode) {
        // This is channel 3, and it's set to be the noise source.
        const int opmNoiseCh = 7; // Noise is always on OPM channel 7

        // 1. Silence the tone output for this OPM channel (ch 7)
        _y(0x08, opmNoiseCh); // Key Off the tone part

        // 2. Control the noise output using this channel's volume
        if (should_be_on) {
            int max_vol = (state->vol_left > state->vol_right) ? state->vol_left : state->vol_right;
            uint8_t pan = OPM_PAN_CENTER;
            if (state->vol_left == 0) pan = OPM_PAN_RIGHT;
            else if (state->vol_right == 0) pan = OPM_PAN_LEFT;

            // Final, most attenuated volume table for noise
            const int NOISE_VOL_TO_TL[] = {127, 125, 122, 119, 116, 113, 110, 107, 104, 101, 98, 95, 92, 89, 86, 83};
            int tl = NOISE_VOL_TO_TL[max_vol & 0xf];
            
            // --- New Noise Frequency Mapping ---
            // The WS noise is clocked by the channel's frequency. We need to map the 11-bit period to OPM's 5-bit noise frequency.
            // A higher WS period means a lower frequency. A higher OPM NF value means a lower frequency.
            // So we can do a rough mapping.
            uint16_t ws_period = state->period;
            int opm_nf;
            if (ws_period > 1800) opm_nf = 31;       // Lowest frequencies
            else if (ws_period > 1600) opm_nf = 30;
            else if (ws_period > 1400) opm_nf = 29;
            else if (ws_period > 1200) opm_nf = 28;
            else if (ws_period > 1000) opm_nf = 26;
            else if (ws_period > 800) opm_nf = 24;
            else if (ws_period > 600) opm_nf = 22;
            else if (ws_period > 400) opm_nf = 18;
            else if (ws_period > 200) opm_nf = 12;
            else if (ws_period > 100) opm_nf = 6;
            else opm_nf = 2;                        // Highest frequencies

            _y(0x0F, 0x80 | opm_nf); // Set noise frequency & enable
            _y(0x20 + opmNoiseCh, pan | 0x3C); // Set panning for noise
            _y(0x78 + opmNoiseCh, tl); // Set volume on C2 (noise slot)
            
            // Crucial: Key ON the noise slot (C2)
            _y(0x08, (0x8 << 3) | opmNoiseCh);

        } else {
            // Mute noise output
            _y(0x78 + opmNoiseCh, 0x7f);
        }
        state->active = should_be_on; // Track noise state
        return; // End processing for this channel
    }

    // --- Regular Tone Logic ---
    // If we are here, it's either channel 0, 1, 2, or channel 3 in tone mode.
    int opmCh = ch + 4;

    // If this is channel 3, and it was previously noise, ensure noise is off.
    if (ch == 3) {
         _y(0x78 + opmCh, 0x7f); // Mute noise on OPM channel 7
    }

    if (should_be_on && !state->active) { // Key On
        state->active = true;
        state->note_on_time = _total_samples;

        double freq = period_to_freq(state->period);
        if (freq == 0.0) { // Don't key on if frequency is 0
            state->active = false;
            return;
        }
        freqToOPMNote(freq, _clock_ratio, &state->last_opm_kc, &state->last_opm_kf);

        _y(0x28 + opmCh, state->last_opm_kc);
        _y(0x30 + opmCh, state->last_opm_kf << 2);

        int total_vol = state->vol_left + state->vol_right;
        uint8_t pan = OPM_PAN_CENTER;
        if (total_vol > 0) {
            if (state->vol_left == 0) pan = OPM_PAN_RIGHT;
            else if (state->vol_right == 0) pan = OPM_PAN_LEFT;
        }
        _y(0x20 + opmCh, pan | 0x3C); // Use ALG 4, consistent with init

        // Final-final tone volume table mapping to OPM TL 15-40
        const int VOL_TO_TL_FINAL[] = {127, 40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 20, 18, 17, 16, 15};
        int max_vol = (state->vol_left > state->vol_right) ? state->vol_left : state->vol_right;
        
        uint8_t tl_val = VOL_TO_TL_FINAL[max_vol & 0xf];
        _y(0x70 + opmCh, tl_val);

        // For ALG=4, M1->C1. We need to key on both slots (1 and 2).
        uint8_t key_on_cmd = (3 << 3) | opmCh;
        _y(0x08, key_on_cmd);
    } else if (!should_be_on && state->active) { // Key Off
        state->active = false;
        _y(0x08, opmCh); // Key Off all slots
    } else if (should_be_on && state->active) { // Update existing note
        const uint32_t VIBRATO_DELAY_SAMPLES = 4410; // 100ms at 44.1kHz

        // Update frequency only after a delay to stabilize initial note recognition
        if (_total_samples >= state->note_on_time + VIBRATO_DELAY_SAMPLES) {
            double freq = period_to_freq(state->period);
            if (freq > 0) {
                uint8_t new_kc, new_kf;
                freqToOPMNote(freq, _clock_ratio, &new_kc, &new_kf);
                if (new_kc != state->last_opm_kc || new_kf != state->last_opm_kf) {
                    state->last_opm_kc = new_kc;
                    state->last_opm_kf = new_kf;
                    _y(0x28 + opmCh, state->last_opm_kc);
                    _y(0x30 + opmCh, state->last_opm_kf << 2);
                    // logging(LOG_LEVEL_DEBUG, "CH%d Pitch Bend, Freq: %.2f Hz", ch, freq);
                }
            }
        }

        // Update volume and panning
        int total_vol = state->vol_left + state->vol_right;
        uint8_t pan = OPM_PAN_CENTER;
        if (total_vol > 0) {
            if (state->vol_left == 0) pan = OPM_PAN_RIGHT;
            else if (state->vol_right == 0) pan = OPM_PAN_LEFT;
        }
        _y(0x20 + opmCh, pan | 0x3C);

        const int VOL_TO_TL_FINAL[] = {127, 40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 20, 18, 17, 16, 15};
        int max_vol = (state->vol_left > state->vol_right) ? state->vol_left : state->vol_right;
        _y(0x70 + opmCh, VOL_TO_TL_FINAL[max_vol & 0xf]);
    }
}


void ws_to_opm_update(uint32_t samples) {
    _total_samples += samples;
    // The main logic is in _update_channel, called from ws_to_opm_write_reg.
    // This function just keeps track of time.
    (void)samples;
}
