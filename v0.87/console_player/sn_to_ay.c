#include "sn_to_ay.h"
#include "ay_to_opm.h" // We will call this directly
#include <string.h>
#include <math.h>

// --- Internal State (based on sn76489-to-ay8910-converter.ts) ---
static uint16_t _freq[4];
static uint8_t _ch;      // channel number latched
static uint8_t _type;    // register type latched (0 for freq, 1 for att)
static uint8_t _atts[4]; // channel attenuations
static int _mixChannel = 2;
static bool _periodic;
static uint8_t _noiseFreq;

// --- Constants ---
static const int voltbl[] = {15, 14, 14, 13, 12, 12, 11, 10, 10, 9, 8, 8, 7, 6, 6, 0};
static const int _noisePitchMap[] = {7, 15, 31};

// --- Helper Functions ---
// This function will write to the *next* converter in the chain (ay_to_opm)
static void _y(uint8_t addr, uint8_t data) {
    ay_to_opm_write_reg(addr, data);
}

static void _updateSharedChannel() {
    int noiseChannel = _mixChannel;
    bool enableTone = _atts[noiseChannel] != 0xf;
    bool enableNoise = _atts[3] != 0xf;
    int att;

    // Simple mix resolver: if both are enabled, noise wins.
    if (enableTone && enableNoise) {
        enableTone = false;
    }

    if (enableTone) {
        att = _atts[noiseChannel];
    } else if (enableNoise) {
        att = _atts[3];
    } else {
        att = _atts[noiseChannel];
    }

    const uint8_t toneMask = enableTone ? 0 : (1 << noiseChannel);
    const uint8_t noiseMask = enableNoise ? (7 & ~(1 << noiseChannel)) : 7;
    _y(7, (noiseMask << 3) | toneMask);
    _y(8 + noiseChannel, voltbl[att & 0xF]);
}

static void _updateAttenuation(int ch, int rawAtt) {
    _atts[ch] = rawAtt & 0xF;
    if (_mixChannel >= 0 && (ch == _mixChannel || ch == 3)) {
        _updateSharedChannel();
    } else if (ch < 3) {
        _y(8 + ch, voltbl[_atts[ch]]);
    }
}

static void _updateNoise(uint8_t data) {
    _periodic = (data & 4) ? false : true;
    _noiseFreq = data & 3;
    _updateSharedChannel();

    if ((data & 3) == 3) {
        _y(6, _freq[2] & 31);
    } else {
        _y(6, _noisePitchMap[data & 3]);
    }
}

static void _updateFreq(int ch) {
    if (ch < 3) {
        uint16_t freq = _freq[ch];
        _y(ch * 2, freq & 0xff);
        _y(ch * 2 + 1, (freq >> 8) & 0x0f);
    }
}

// --- Public API ---
void sn_to_ay_init(chip_type_t source_chip_type, uint32_t source_clock) {
    (void)source_chip_type; // Unused
    (void)source_clock;   // Unused

    memset(_freq, 0, sizeof(_freq));
    _ch = 0;
    _type = 0;
    for(int i=0; i<4; i++) _atts[i] = 0xf;
    _periodic = false;
    _noiseFreq = 0;

    // Initialize the next converter in the chain
    // The clock doesn't matter as much here since we are just passing AY register writes
    ay_to_opm_init(CHIP_TYPE_AY8910, 1789773); 

    // Initial AY8910 state
    _y(7, 0x38); // Disable I/O, enable Tone channels 0,1,2, disable noise
}

void sn_to_ay_write_reg(uint8_t data) {
    if (data & 0x80) {
        _ch = (data >> 5) & 3;
        _type = (data >> 4) & 1;
        if (_type) { // Attenuation
            _updateAttenuation(_ch, data & 0xf);
        } else { // Frequency (lower 4 bits)
            if (_ch < 3) {
                _freq[_ch] = (_freq[_ch] & 0x3f0) | (data & 0xf);
            } else {
                _updateNoise(data);
            }
            _updateFreq(_ch);
        }
    } else { // Data byte (upper 6 bits of frequency)
        if (_type == 0 && _ch < 3) {
            _freq[_ch] = ((data & 0x3f) << 4) | (_freq[_ch] & 0xf);
            _updateFreq(_ch);
        }
        // Data bytes for attenuation are ignored, as per SN76489 behavior
    }
}
