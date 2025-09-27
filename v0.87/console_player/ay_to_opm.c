#include "ay_to_opm.h"
#include "spfm.h"
#include "chiptype.h"
#include "ym2151.h" // To call ym2151_write_reg directly
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

// --- Constants from vgm-conv-main ---
static const int VOL_TO_TL[] = {127, 62, 56, 52, 46, 42, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0};
static const int N_VOL_TO_TL[] = {127, 126, 125, 124, 123, 122, 121, 120, 116, 112, 105, 96, 82, 64, 37, 0};
static const int OPM_CH_BASE = 4; // Use channels 4, 5, 6 for PSG tones, 7 for noise

// --- Helper Functions ---
static void _y(uint8_t addr, uint8_t data) {
    // We call ym2151_write_reg directly to bypass any clock conversion in ym2151.c
    ym2151_write_reg(_opm_slot, addr, data);
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
    freqToOPMNote(freq, _clock_ratio / _fdiv, &kc, &kf);
    int opmCh = toOpmCh(ch);
    _y(0x28 + opmCh, kc);
    _y(0x30 + opmCh, kf << 2);
}

static void _updateNoise() {
    int nVol = 0;
    for (int i = 0; i < 3; i++) {
        if ((_regs[7] & (0x8 << i)) == 0) {
            nVol = fmax(nVol, _regs[8 + i] & 0xf);
        }
    }
    int nfreq = _regs[6] & 0x1f;
    _y(0x0f, 0x80 | (0x1f - nfreq));
    _y(0x7f, fmin(127, N_VOL_TO_TL[nVol])); // SLOT32, attenuation handled later if needed
}

static void _updateTone(int ch) {
    const int t = ((1 << ch) & _regs[7]) == 0;
    int opmCh = toOpmCh(ch);
    if (t) {
        const int v = _regs[8 + ch];
        const int tVol = (v & 0x10) ? 0 : (v & 0xf);
        _y(0x70 + opmCh, fmin(127, VOL_TO_TL[tVol & 0xf]));
    } else {
        _y(0x70 + opmCh, 0x7f);
    }
}

// --- Public API ---

void ay_to_opm_init(chip_type_t source_chip_type, uint32_t source_clock) {
    _source_chip = source_chip_type;
    _source_clock = source_clock;
    _opm_slot = get_slot_for_chip(CHIP_TYPE_YM2151);

    const double OPM_CLOCK = get_chip_default_clock(CHIP_TYPE_YM2151);
    _clock_ratio = 3579545.0 / OPM_CLOCK;
    _fdiv = (_source_chip == CHIP_TYPE_AY8910) ? 2 : 4;

    memset(_regs, 0, sizeof(_regs));

    // --- Initial Commands ---
    // PSG TONE Channels (4, 5, 6)
    for (int opmCh = OPM_CH_BASE; opmCh < OPM_CH_BASE + 3; opmCh++) {
        _y(0x20 + opmCh, 0xfc); // RL=ON FB=7 CON=4
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
    spfm_flush();
}

void ay_to_opm_write_reg(uint8_t addr, uint8_t data) {
    if (addr > 15) return;
    _regs[addr] = data;

    if (addr <= 5) { // Tone period
        const int ch = addr >> 1;
        const int tp = ((_regs[ch * 2 + 1] & 0x0F) << 8) | _regs[ch * 2];
        if (tp == 0) {
            _updateFreq(ch, 0);
        } else {
            const double freq = (double)_source_clock / (16 * tp);
            _updateFreq(ch, freq);
        }
    } else if (addr >= 8 && addr <= 10) { // Amplitudes
        _updateTone(addr - 8);
        _updateNoise();
    } else if (addr == 6) { // Noise period
        _updateNoise();
    } else if (addr == 7) { // Mixer
        _updateTone(0);
        _updateTone(1);
        _updateTone(2);
        _updateNoise();
    }
    // Envelope registers (11, 12, 13) are ignored for now.
    
    spfm_flush();
}
