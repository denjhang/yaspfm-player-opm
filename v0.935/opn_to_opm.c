#include "opn_to_opm.h"
#include "spfm.h"
#include "chiptype.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// --- Global State for LFO Amplitude ---
volatile double g_opn_lfo_amplitude = 0.90;

// --- Internal State ---
static uint8_t _regs[2][256]; // Registers for 2 ports
static double _clock_ratio;
static double _clock_div;
static uint32_t _source_clock;
static chip_type_t _source_chip;
static uint8_t _opm_slot; // This is now only used for spfm_flush, can be removed later
static uint8_t _lr_cache[8];
static opm_write_func_t _write_func;

// --- Accurate Frequency Conversion based on vgm-conv-main ---
const double BASE_FREQ_OPM = 277.2; // C#4 = 60
const int KEY_TO_NOTE_OPM[] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14};

static void freq_to_opm_note(double freq, double clockRatio, uint8_t* kc, uint8_t* kf) {
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

static double fnum_to_freq(uint16_t fnum, uint8_t blk) {
    return (_source_clock * fnum) / ((72.0 * _clock_div) * (1 << (20 - blk)));
}

static void opn_freq_to_opm_key(uint16_t fnum, uint8_t blk, uint8_t* key_code, uint8_t* key_fraction) {
    double freq = fnum_to_freq(fnum, blk);
    freq_to_opm_note(freq, _clock_ratio, key_code, key_fraction);
}

static uint8_t get_rl_flags(uint8_t ch) {
    if (_source_chip == CHIP_TYPE_YM2203) {
        return 3;
    }
    uint8_t lr = _lr_cache[ch];
    return ((lr & 1) << 1) | ((lr >> 1) & 1);
}

// --- Public API ---

void opn_to_opm_init(chip_type_t source_chip_type, uint32_t source_clock, opm_write_func_t write_func) {
    _source_chip = source_chip_type;
    _source_clock = source_clock;
    _opm_slot = get_slot_for_chip(CHIP_TYPE_YM2151);
    _write_func = write_func;

    const double OPM_CLOCK = 3579545.0;
    _clock_ratio = OPM_CLOCK / get_chip_default_clock(CHIP_TYPE_YM2151);

    if (_source_chip == CHIP_TYPE_YM2203) {
        _clock_div = 1.0;
    } else { // YM2608, YM2612
        _clock_div = 2.0;
    }

    // Reset internal state
    memset(_regs, 0, sizeof(_regs));
    for (int i = 0; i < 8; i++) {
        _lr_cache[i] = 3; // Default to L/R on, as per YM2612 default
    }
    
    // Initial commands from OPNFMOnSFG.asm
    if (_write_func) {
        _write_func(0x19, 0x10); // AMD, rescales AMS
        _write_func(0x19, 0xA8); // PMD, rescales PMS
        _write_func(0x1B, 0x02); // W, triangle LFO waveform
    }
}

void opn_to_opm_write_reg(uint8_t addr, uint8_t data, uint8_t port) {
    if (port > 1 || !_write_func) return;

    _regs[port][addr] = data;

    if (port == 0) {
        if (addr == 0x22 && _source_chip != CHIP_TYPE_YM2203) {
            // OPNFMOnSFG_lfoLUT
            const uint8_t lfo_lut[] = {0, 0, 0, 0, 0, 0, 0, 0, 0xC1, 0xC7, 0xC9, 0xCB, 0xCD, 0xD4, 0xF9, 0xFF};
            _write_func(0x18, lfo_lut[data & 0x0F]);
        } else if (addr == 0x28) {
            // YM2612 CH: 0,1,2,4,5,6 -> OPM CH: 0,1,2,3,4,5
            uint8_t opn_ch = data & 0x07;
            if (opn_ch == 3 || opn_ch > 6) return;
            uint8_t opm_ch = (opn_ch < 3) ? opn_ch : (opn_ch - 1);
            uint8_t slots = (data & 0xf0) >> 4;
            _write_func(0x08, (slots << 3) | opm_ch);
        }
    }

    if (addr >= 0x30 && addr <= 0x8f) {
        uint8_t nch = addr & 3;
        if (port == 0 && nch == 3) return; // YM2612 Port 0 only has ch 0,1,2
        uint8_t ch = (port == 0 ? 0 : 3) + nch;
        uint8_t slot = (addr >> 2) & 3;
        uint8_t base = 0x40 + ((addr & 0xf0) - 0x30) * 2;
        uint8_t offset = slot * 8 + ch;
        _write_func(base + offset, data);
    }

    if (addr >= 0xb0 && addr <= 0xb2) { // FB CON
        uint8_t nch = addr & 3;
        if (port == 0 && nch == 3) return;
        uint8_t ch = (port == 0 ? 0 : 3) + nch;
        _write_func(0x20 + ch, (get_rl_flags(ch) << 6) | (data & 0x3f));
    }

    if (addr >= 0xb4 && addr <= 0xb6) { // L/R AMS PMS
        uint8_t nch = addr & 3;
        if (port == 0 && nch == 3) return;
        uint8_t ch = (port == 0 ? 0 : 3) + nch;
        _lr_cache[ch] = (data >> 6) & 0x3;
        uint8_t ams = (data >> 4) & 0x3;
        uint8_t pms = data & 0x7;
        
        // Apply LFO amplitude scaling
        uint8_t scaled_pms = (uint8_t)(pms * g_opn_lfo_amplitude);
        if (scaled_pms > 7) scaled_pms = 7;

        _write_func(0x38 + ch, (scaled_pms << 4) | ams);
        _write_func(0x20 + ch, (get_rl_flags(ch) << 6) | (_regs[port][0xb0 + nch] & 0x3f));
    }

    if ((addr >= 0xa0 && addr <= 0xa2) || (addr >= 0xa4 && addr <= 0xa6)) {
        uint8_t nch = addr & 3;
        if (port == 0 && nch == 3) return;
        uint8_t ch = (port == 0 ? 0 : 3) + nch;
        uint8_t al = 0xa0 + nch;
        uint8_t ah = 0xa4 + nch;
        uint16_t fnum = (((_regs[port][ah] & 7) << 8) | _regs[port][al]) >> 2;
        uint8_t blk = (_regs[port][ah] >> 3) & 7;
        uint8_t kc, kf;
        opn_freq_to_opm_key(fnum, blk, &kc, &kf);
        _write_func(0x28 + ch, kc);
        _write_func(0x30 + ch, kf << 2);
    }
}
