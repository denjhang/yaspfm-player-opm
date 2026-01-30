#include "saa1099_viz.h"
#include "saa1099.h"
#include "util.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// Global state
static saa1099_state_t g_saa_states[2]; // Support up to 2 slots
static HWND g_hwnd[2] = {NULL, NULL};  // One window per slot
static HANDLE g_viz_thread = NULL;
static volatile bool g_viz_running = false;
static volatile bool g_viz_initialized = false;
static CRITICAL_SECTION g_viz_lock;

// Window constants
static const int WINDOW_WIDTH = 1320;  // Increased to fit dual VU meters, NOISE and SQUARE status boxes
static const int WINDOW_HEIGHT = 720;  // Single chip (6 channels)
static const int CHANNEL_HEIGHT = 110;
static const int TOP_MARGIN = 50;
static const int LEFT_MARGIN = 20;

// Piano constants
static const int PIANO_WIDTH = 840;  // 8 octaves, wider keys
static const int PIANO_HEIGHT = 50;
static const int WHITE_KEY_WIDTH = 15;  // Wider for better visibility
static const int BLACK_KEY_WIDTH = 10;
static const int BLACK_KEY_HEIGHT = 30;
static const int OCTAVES = 8;  // 8 octaves
static const int WHITE_KEYS_PER_OCTAVE = 7;

// VU meter constants
static const int VU_WIDTH = 35;
static const int VU_HEIGHT = 50;

// Envelope display constants (waveform + text separated)
static const int ENV_DISPLAY_WIDTH = 150;  // Increased for separate areas
static const int ENV_DISPLAY_HEIGHT = 50;
static const int ENV_WAVEFORM_WIDTH = 60;  // Left part: waveform
static const int ENV_TEXT_WIDTH = 85;      // Right part: text info

// Channel status display constants
static const int STATUS_BOX_WIDTH = 70;
static const int STATUS_BOX_HEIGHT = 50;

// Colors
static const COLORREF COLOR_BG = RGB(18, 18, 24);
static const COLORREF COLOR_TEXT = RGB(220, 220, 230);
static const COLORREF COLOR_TEXT_DIM = RGB(140, 140, 150);
static const COLORREF COLOR_PANEL = RGB(30, 30, 40);
static const COLORREF COLOR_BORDER = RGB(60, 60, 80);
static const COLORREF COLOR_CHANNEL_ACTIVE = RGB(100, 220, 120);
static const COLORREF COLOR_CHANNEL_MUTE = RGB(80, 80, 90);
static const COLORREF COLOR_KEY_WHITE = RGB(240, 240, 245);
static const COLORREF COLOR_KEY_BLACK = RGB(35, 35, 40);
static const COLORREF COLOR_KEY_ACTIVE_LOW = RGB(100, 160, 255);
static const COLORREF COLOR_KEY_ACTIVE_HIGH = RGB(50, 100, 255);
static const COLORREF COLOR_KEY_ENVELOPE = RGB(80, 220, 100);  // Green for envelope tones
static const COLORREF COLOR_KEY_NOISE = RGB(120, 120, 130);    // Gray for noise
static const COLORREF COLOR_VU_BG = RGB(40, 40, 50);
static const COLORREF COLOR_VU_GREEN = RGB(80, 220, 100);
static const COLORREF COLOR_VU_YELLOW = RGB(255, 220, 80);
static const COLORREF COLOR_VU_RED = RGB(255, 80, 80);

// Piano note mapping (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
static const bool is_black_key[] = {false, true, false, true, false, false, true, false, true, false, true, false};
static const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Calculate frequency in Hz from SAA1099 parameters
// Reference: libvgm saa1099_mame.c lines 301-302, 309-310
static double calc_frequency(uint8_t freq_val, uint8_t octave) {
    if (freq_val == 0) return 0.0;

    // SAA1099 frequency formula from libvgm:
    // freq = clk2div512 * (1 << octave) / (511 - frequency_value)
    // where clk2div512 = (master_clock + 128) / 256
    const double master_clock = 8000000.0;  // 8 MHz
    double clk2div512 = (master_clock + 128.0) / 256.0;

    double divisor = 511.0 - freq_val;
    if (divisor <= 0.0) return 0.0;

    return (clk2div512 * (1 << octave)) / divisor;
}

// Get stereo mode for a channel (left/right enable)
static void get_stereo_info(const saa1099_state_t* state, int channel, bool* left, bool* right) {
    // SAA1099 has no direct stereo registers - all channels are stereo
    // But we can check if noise control affects stereo
    // For now, assume both channels are always enabled
    *left = true;
    *right = true;
}

// Get octave value for specific channel
static uint8_t get_channel_octave(const saa1099_state_t* state, int channel) {
    int oct_reg = channel / 2;
    int oct_shift = (channel % 2) * 4;
    return (state->octave[oct_reg] >> oct_shift) & 0x07;
}

// Convert frequency to MIDI note number (approximate)
static int freq_to_midi_note(double freq) {
    if (freq <= 0.0) return -1;
    // MIDI note = 69 + 12 * log2(freq / 440)
    double note = 69.0 + 12.0 * log2(freq / 440.0);
    return (int)(note + 0.5);
}

// Calculate noise frequency from SAA1099 noise control register
// Reference: SAA1099 noise generator uses tone generator 0 or internal clock
static double calc_noise_frequency(const saa1099_state_t* state) {
    // Noise control register (0x16):
    // Bit 4: Noise clock source (0=tone gen 0, 1=internal)
    // Bits 1-0: Noise frequency divider
    uint8_t noise_ctrl = state->noise_control;
    bool use_tone_gen = (noise_ctrl & 0x10) == 0;

    if (use_tone_gen) {
        // Use tone generator 0 frequency
        uint8_t freq_val = state->frequency[0];
        uint8_t octave = state->octave[0] & 0x07;
        return calc_frequency(freq_val, octave);
    } else {
        // Internal noise clock - approximate based on master clock
        // SAA1099 noise generator runs at master_clock / (2^(divider+1))
        uint8_t divider = noise_ctrl & 0x03;
        const double master_clock = 8000000.0;
        double noise_freq = master_clock / (2 << (divider + 4));  // Rough approximation
        return noise_freq;
    }
}

// Tone type for piano key coloring
enum ToneType {
    TONE_NORMAL,     // Normal tone (blue)
    TONE_ENVELOPE,   // Envelope tone (green)
    TONE_NOISE       // Noise tone (gray)
};

// Structure to hold multiple notes to display
typedef struct {
    int midi_note;
    int intensity;
    ToneType tone_type;
} NoteInfo;

// Get interpolated color for a tone type and intensity
static COLORREF get_tone_color(ToneType tone_type, int intensity, bool is_black) {
    COLORREF base_color;
    switch (tone_type) {
        case TONE_ENVELOPE:
            base_color = COLOR_KEY_ENVELOPE;  // Green for envelope
            break;
        case TONE_NOISE:
            base_color = COLOR_KEY_NOISE;     // Gray for noise
            break;
        default:
            base_color = is_black ? RGB(50, 100, 255) : RGB(100, 160, 255);  // Blue for normal
            break;
    }

    // Interpolate intensity
    int base_r = GetRValue(base_color);
    int base_g = GetGValue(base_color);
    int base_b = GetBValue(base_color);

    if (is_black) {
        int r = 35 + (base_r - 35) * intensity / 15;
        int g = 35 + (base_g - 35) * intensity / 15;
        int b = 40 + (base_b - 40) * intensity / 15;
        return RGB(r, g, b);
    } else {
        int r = 200 + (base_r - 200) * intensity / 15;
        int g = 200 + (base_g - 200) * intensity / 15;
        int b = 200 + (base_b - 200) * intensity / 15;
        return RGB(r, g, b);
    }
}

// Draw a piano key with multiple tone types (split into segments)
static void draw_piano_key_multi(HDC hdc, int x, int y, int width, int height, bool is_black,
                                const NoteInfo* tones, int tone_count) {
    HPEN pen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);

    if (tone_count == 0 || (tone_count == 1 && tones[0].intensity == 0)) {
        // No active tones - draw default key
        COLORREF color = is_black ? COLOR_KEY_BLACK : COLOR_KEY_WHITE;
        HBRUSH brush = CreateSolidBrush(color);
        HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
        Rectangle(hdc, x, y, x + width, y + height);
        SelectObject(hdc, old_brush);
        DeleteObject(brush);
    } else if (tone_count == 1) {
        // Single tone - draw as solid color
        COLORREF color = get_tone_color(tones[0].tone_type, tones[0].intensity, is_black);
        HBRUSH brush = CreateSolidBrush(color);
        HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
        Rectangle(hdc, x, y, x + width, y + height);
        SelectObject(hdc, old_brush);
        DeleteObject(brush);
    } else {
        // Multiple tones - split key into segments
        int segment_height = height / tone_count;

        for (int i = 0; i < tone_count; i++) {
            int seg_y = y + i * segment_height;
            int seg_height = (i == tone_count - 1) ? (height - i * segment_height) : segment_height;

            COLORREF color = get_tone_color(tones[i].tone_type, tones[i].intensity, is_black);
            HBRUSH brush = CreateSolidBrush(color);
            HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);

            Rectangle(hdc, x, seg_y, x + width, seg_y + seg_height);

            SelectObject(hdc, old_brush);
            DeleteObject(brush);
        }
    }

    SelectObject(hdc, old_pen);
    DeleteObject(pen);
}

// Draw piano keyboard with multiple highlighted notes
// Display range: C3 to B10 (8 octaves, MIDI notes 48-143)
static void draw_piano_multi(HDC hdc, int x, int y, const NoteInfo* notes, int note_count) {
    const int START_OCTAVE = 3;  // Start from octave 3 (C3)

    // Draw white keys first
    int white_key_x = x;
    for (int octave = START_OCTAVE; octave < START_OCTAVE + OCTAVES; octave++) {
        for (int note = 0; note < 12; note++) {
            if (!is_black_key[note]) {
                int global_note = octave * 12 + note;

                // Collect all tones that match this key
                NoteInfo key_tones[3];  // Max 3: TONE, ENV, NOISE (may share same pitch)
                int key_tone_count = 0;

                for (int i = 0; i < note_count; i++) {
                    if (notes[i].midi_note == global_note && notes[i].intensity > 0) {
                        key_tones[key_tone_count++] = notes[i];
                    }
                }

                draw_piano_key_multi(hdc, white_key_x, y, WHITE_KEY_WIDTH, PIANO_HEIGHT,
                                   false, key_tones, key_tone_count);
                white_key_x += WHITE_KEY_WIDTH;
            }
        }
    }

    // Draw black keys on top
    white_key_x = x;
    for (int octave = START_OCTAVE; octave < START_OCTAVE + OCTAVES; octave++) {
        for (int note = 0; note < 12; note++) {
            if (is_black_key[note]) {
                int global_note = octave * 12 + note;

                // Collect all tones that match this key
                NoteInfo key_tones[3];  // Max 3: TONE, ENV, NOISE (may share same pitch)
                int key_tone_count = 0;

                for (int i = 0; i < note_count; i++) {
                    if (notes[i].midi_note == global_note && notes[i].intensity > 0) {
                        key_tones[key_tone_count++] = notes[i];
                    }
                }

                int black_x = white_key_x - BLACK_KEY_WIDTH / 2;
                draw_piano_key_multi(hdc, black_x, y, BLACK_KEY_WIDTH, BLACK_KEY_HEIGHT,
                                   true, key_tones, key_tone_count);
            }
            if (!is_black_key[note]) {
                white_key_x += WHITE_KEY_WIDTH;
            }
        }
    }

    // Draw C note labels (C3, C4, ..., C10)
    SetTextColor(hdc, COLOR_TEXT_DIM);
    SetBkMode(hdc, TRANSPARENT);
    white_key_x = x;
    for (int octave = START_OCTAVE; octave < START_OCTAVE + OCTAVES; octave++) {
        // Draw label at the start of each octave (C note)
        char label[8];
        sprintf(label, "C%d", octave);
        TextOut(hdc, white_key_x + 2, y + PIANO_HEIGHT - 12, label, strlen(label));
        // Move to next octave (7 white keys per octave)
        white_key_x += WHITE_KEY_WIDTH * WHITE_KEYS_PER_OCTAVE;
    }
}

// Draw VU meter with label
static void draw_vu_meter(HDC hdc, int x, int y, int amplitude, const char* label) {
    // Background
    HBRUSH bg_brush = CreateSolidBrush(COLOR_VU_BG);
    RECT bg_rect = {x, y, x + VU_WIDTH, y + VU_HEIGHT};
    FillRect(hdc, &bg_rect, bg_brush);
    DeleteObject(bg_brush);

    // Border
    HPEN border_pen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HPEN old_pen = (HPEN)SelectObject(hdc, border_pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x + VU_WIDTH, y + VU_HEIGHT);

    // Draw label at top
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COLOR_TEXT);
    SIZE text_size;
    GetTextExtentPoint32(hdc, label, strlen(label), &text_size);
    int label_x = x + (VU_WIDTH - text_size.cx) / 2;
    TextOut(hdc, label_x, y + 2, label, strlen(label));

    SelectObject(hdc, old_pen);
    DeleteObject(border_pen);

    // Draw LED segments (15 levels)
    if (amplitude > 0) {
        int segments = amplitude; // 0-15 amplitude maps to 0-15 segments directly
        for (int i = 0; i < segments; i++) {
            COLORREF segment_color;
            if (i < 10) {
                segment_color = COLOR_VU_GREEN;
            } else if (i < 13) {
                segment_color = COLOR_VU_YELLOW;
            } else {
                segment_color = COLOR_VU_RED;
            }

            int segment_height = 2;
            int segment_y = y + VU_HEIGHT - 4 - (i * 3);
            HBRUSH seg_brush = CreateSolidBrush(segment_color);
            RECT seg_rect = {x + 3, segment_y, x + VU_WIDTH - 3, segment_y + segment_height};
            FillRect(hdc, &seg_rect, seg_brush);
            DeleteObject(seg_brush);
        }
    }
}

// Draw hardware envelope waveform (SAA1099 envelope generator visualization)
// Layout: [Waveform Area (60px)] | [Text Info Area (85px)]
static void draw_hardware_envelope(HDC hdc, int x, int y, uint8_t env_value, int env_num) {
    // SAA1099 Envelope bits (register 0x18/0x19):
    // bit 7: Enable (1=on, 0=off)
    // bit 4: Resolution (1=3-bit, 0=4-bit)
    // bits 3-1: Mode (0-7)

    bool enabled = (env_value & 0x80) != 0;
    bool resolution = (env_value & 0x10) != 0;  // 1=3bit, 0=4bit
    uint8_t mode = (env_value >> 1) & 0x07;

    const char* mode_names[] = {
        "Down",      // Mode 0: Saw down repeating
        "Tri",       // Mode 1: Triangle repeating
        "Down1",     // Mode 2: Saw down once
        "Tri1",      // Mode 3: Triangle once
        "Up",        // Mode 4: Saw up repeating
        "TriInv",    // Mode 5: Triangle inverted repeating
        "Up1",       // Mode 6: Saw up once
        "TriInv1"    // Mode 7: Triangle inverted once
    };

    // Draw overall border
    HPEN border_pen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HPEN old_pen = (HPEN)SelectObject(hdc, border_pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x + ENV_DISPLAY_WIDTH, y + ENV_DISPLAY_HEIGHT);

    // === LEFT SIDE: Waveform Area ===
    int waveform_x = x;
    int waveform_y = y;

    // Draw separator between waveform and text
    MoveToEx(hdc, x + ENV_WAVEFORM_WIDTH, y, NULL);
    LineTo(hdc, x + ENV_WAVEFORM_WIDTH, y + ENV_DISPLAY_HEIGHT);

    if (enabled) {
        // Draw envelope waveform
        HPEN env_pen = CreatePen(PS_SOLID, 2, RGB(100, 200, 255));
        SelectObject(hdc, env_pen);

        int max_steps = resolution ? 8 : 16;
        int step_width = (ENV_WAVEFORM_WIDTH - 10) / max_steps;
        int max_height = ENV_DISPLAY_HEIGHT - 10;
        int start_x = waveform_x + 5;
        int base_y = waveform_y + ENV_DISPLAY_HEIGHT - 5;

        for (int step = 0; step < max_steps; step++) {
            int x1 = start_x + step * step_width;
            int x2 = start_x + (step + 1) * step_width;
            int h1 = 0, h2 = 0;

            switch (mode) {
                case 0: // Saw down repeating
                    h1 = max_height - (step * max_height / (max_steps - 1));
                    h2 = max_height - ((step + 1) % max_steps * max_height / (max_steps - 1));
                    break;
                case 1: // Triangle repeating
                    if (step < max_steps / 2) {
                        h1 = step * max_height / (max_steps / 2);
                        h2 = (step + 1) * max_height / (max_steps / 2);
                    } else {
                        h1 = max_height - (step - max_steps / 2) * max_height / (max_steps / 2);
                        h2 = max_height - (step + 1 - max_steps / 2) * max_height / (max_steps / 2);
                    }
                    break;
                case 2: // Saw down once
                    if (step < max_steps - 1) {
                        h1 = max_height - (step * max_height / (max_steps - 1));
                        h2 = max_height - ((step + 1) * max_height / (max_steps - 1));
                    } else {
                        h1 = 0;
                        h2 = 0;
                    }
                    break;
                case 3: // Triangle once
                    if (step < max_steps / 2) {
                        h1 = step * max_height / (max_steps / 2);
                        h2 = (step + 1) * max_height / (max_steps / 2);
                    } else if (step < max_steps - 1) {
                        h1 = max_height - (step - max_steps / 2) * max_height / (max_steps / 2);
                        h2 = max_height - (step + 1 - max_steps / 2) * max_height / (max_steps / 2);
                    } else {
                        h1 = 0;
                        h2 = 0;
                    }
                    break;
                case 4: // Saw up repeating
                    h1 = (step * max_height / (max_steps - 1));
                    h2 = ((step + 1) % max_steps * max_height / (max_steps - 1));
                    break;
                case 5: // Triangle inverted repeating
                    if (step < max_steps / 2) {
                        h1 = max_height - step * max_height / (max_steps / 2);
                        h2 = max_height - (step + 1) * max_height / (max_steps / 2);
                    } else {
                        h1 = (step - max_steps / 2) * max_height / (max_steps / 2);
                        h2 = (step + 1 - max_steps / 2) * max_height / (max_steps / 2);
                    }
                    break;
                case 6: // Saw up once
                    if (step < max_steps - 1) {
                        h1 = step * max_height / (max_steps - 1);
                        h2 = (step + 1) * max_height / (max_steps - 1);
                    } else {
                        h1 = max_height;
                        h2 = max_height;
                    }
                    break;
                case 7: // Triangle inverted once
                    if (step < max_steps / 2) {
                        h1 = max_height - step * max_height / (max_steps / 2);
                        h2 = max_height - (step + 1) * max_height / (max_steps / 2);
                    } else if (step < max_steps - 1) {
                        h1 = (step - max_steps / 2) * max_height / (max_steps / 2);
                        h2 = (step + 1 - max_steps / 2) * max_height / (max_steps / 2);
                    } else {
                        h1 = 0;
                        h2 = 0;
                    }
                    break;
            }

            MoveToEx(hdc, x1, base_y - h1, NULL);
            LineTo(hdc, x2, base_y - h2);
        }

        SelectObject(hdc, old_pen);
        DeleteObject(env_pen);
    } else {
        // Draw "OFF" text in waveform area
        SetTextColor(hdc, COLOR_TEXT_DIM);
        SetBkMode(hdc, TRANSPARENT);
        TextOut(hdc, waveform_x + 15, waveform_y + 18, "OFF", 3);
    }

    // === RIGHT SIDE: Text Info Area ===
    int text_x = x + ENV_WAVEFORM_WIDTH + 5;
    int text_y = y + 3;

    SetBkMode(hdc, TRANSPARENT);

    // Line 1: ENV header
    SetTextColor(hdc, COLOR_TEXT);
    char line1[32];
    sprintf(line1, "ENV%d:", env_num);
    TextOut(hdc, text_x, text_y, line1, strlen(line1));

    // Line 2: Register value
    SetTextColor(hdc, COLOR_TEXT_DIM);
    char line2[32];
    sprintf(line2, "0x%02X", env_value);
    TextOut(hdc, text_x, text_y + 12, line2, strlen(line2));

    if (enabled) {
        // Line 3: Mode name
        char line3[32];
        sprintf(line3, "M%d:%s", mode, mode_names[mode]);
        TextOut(hdc, text_x, text_y + 24, line3, strlen(line3));

        // Line 4: Resolution
        char line4[16];
        sprintf(line4, "%dbit", resolution ? 3 : 4);
        TextOut(hdc, text_x, text_y + 36, line4, strlen(line4));
    }

    SelectObject(hdc, old_pen);
    DeleteObject(border_pen);
}

// Draw channel status box (NOISE or SQUARE tone)
static void draw_status_box(HDC hdc, int x, int y, const char* label, bool is_active) {
    // Draw border
    HPEN border_pen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HPEN old_pen = (HPEN)SelectObject(hdc, border_pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x + STATUS_BOX_WIDTH, y + STATUS_BOX_HEIGHT);

    // Draw label and status
    SetBkMode(hdc, TRANSPARENT);

    // Label at top
    SetTextColor(hdc, COLOR_TEXT);
    char display_text[32];
    sprintf(display_text, "%s:", label);
    TextOut(hdc, x + 5, y + 5, display_text, strlen(display_text));

    // Status in center
    const char* status_text = is_active ? label : "OFF";
    COLORREF status_color = is_active ? RGB(100, 220, 120) : COLOR_TEXT_DIM;
    SetTextColor(hdc, status_color);

    // Center the status text
    SIZE text_size;
    GetTextExtentPoint32(hdc, status_text, strlen(status_text), &text_size);
    int text_x = x + (STATUS_BOX_WIDTH - text_size.cx) / 2;
    int text_y = y + (STATUS_BOX_HEIGHT - text_size.cy) / 2 + 5;
    TextOut(hdc, text_x, text_y, status_text, strlen(status_text));

    SelectObject(hdc, old_pen);
    DeleteObject(border_pen);
}

// Draw register info (two lines above piano)
static void draw_register_info(HDC hdc, int x, int y, const saa1099_state_t* state, int channel) {
    char text[256];
    uint8_t amp = state->amplitude[channel];
    uint8_t freq = state->frequency[channel];
    uint8_t oct = get_channel_octave(state, channel);
    bool ch_en = (state->channel_enable & (1 << channel)) != 0;
    bool noise_en = (state->noise_enable & (1 << channel)) != 0;
    uint8_t env0 = state->envelope[0];
    uint8_t env1 = state->envelope[1];

    // SAA1099 amplitude register format (bits 7-0):
    // Reference: libvgm saa1099_mame.c lines 14-25
    // Bits 3-0: Left channel amplitude (0-15)
    // Bits 7-4: Right channel amplitude (0-15)
    uint8_t amp_left = amp & 0x0F;
    uint8_t amp_right = (amp >> 4) & 0x0F;
    bool stereo_left = (amp_left > 0);
    bool stereo_right = (amp_right > 0);
    uint8_t amp_level = (amp_left > amp_right) ? amp_left : amp_right;  // Use max for display

    SetTextColor(hdc, COLOR_TEXT);

    // Line 1: Main register info with stereo L/R
    char stereo_str[8] = "";
    if (stereo_left && stereo_right) {
        strcpy(stereo_str, " LR");
    } else if (stereo_left) {
        strcpy(stereo_str, " L");
    } else if (stereo_right) {
        strcpy(stereo_str, " R");
    } else {
        strcpy(stereo_str, " --");
    }

    sprintf(text, "CH%d  Amp:0x%02X(%2d)%s  Freq:0x%02X  Oct:%d  %s%s",
           channel, amp, amp_level, stereo_str, freq, oct,
           ch_en ? "EN" : "MUTE",
           noise_en ? " +NOISE" : "");
    TextOut(hdc, x, y, text, strlen(text));

    // Line 2: Envelope info
    SetTextColor(hdc, COLOR_TEXT_DIM);
    sprintf(text, "Env0:0x%02X Env1:0x%02X", env0, env1);
    TextOut(hdc, x, y + 14, text, strlen(text));
}

// Draw a complete channel visualization
static void draw_channel(HDC hdc, int channel, int y_pos, const saa1099_state_t* state) {
    // Get channel parameters
    uint8_t amp_reg = state->amplitude[channel];
    uint8_t amp_left = amp_reg & 0x0F;
    uint8_t amp_right = (amp_reg >> 4) & 0x0F;
    uint8_t amp = (amp_left > amp_right) ? amp_left : amp_right;  // Use max for display
    uint8_t freq_val = state->frequency[channel];
    uint8_t octave = get_channel_octave(state, channel);
    bool ch_enabled = (state->channel_enable & (1 << channel)) != 0;
    bool noise_enabled = (state->noise_enable & (1 << channel)) != 0;

    // Check if envelope is enabled for this channel
    // Envelope 0 controls channels 0, 1, 4
    // Envelope 1 controls channels 2, 3, 5
    bool env_active = false;
    if (channel == 0 || channel == 1 || channel == 4) {
        env_active = (state->envelope[0] & 0x80) != 0;  // Envelope 0 enable bit
    } else if (channel == 2 || channel == 3 || channel == 5) {
        env_active = (state->envelope[1] & 0x80) != 0;  // Envelope 1 enable bit
    }

    // Collect all active notes for this channel
    // Note: Normal tone and envelope may share the same frequency/MIDI note
    NoteInfo notes[3];
    int note_count = 0;

    // Track which tone types are active
    bool has_normal = false;
    bool has_envelope = false;
    bool has_noise = false;
    int tone_midi_note = -1;  // MIDI note for tone/envelope

    // Check for normal tone
    if (ch_enabled && freq_val > 0) {
        double freq_hz = calc_frequency(freq_val, octave);
        tone_midi_note = freq_to_midi_note(freq_hz);
        if (tone_midi_note >= 0) {
            has_normal = true;
        }
    }

    // Check for envelope tone (even if channel is muted)
    if (env_active && freq_val > 0) {
        double freq_hz = calc_frequency(freq_val, octave);
        int env_midi_note = freq_to_midi_note(freq_hz);
        if (env_midi_note >= 0) {
            has_envelope = true;
            if (tone_midi_note < 0) {
                tone_midi_note = env_midi_note;
            }
        }
    }

    // Add tone/envelope note (they share the same MIDI note)
    if (tone_midi_note >= 0) {
        notes[note_count].midi_note = tone_midi_note;
        // Prefer envelope color if active, otherwise normal
        if (has_envelope) {
            notes[note_count].intensity = amp > 0 ? amp : 8;
            notes[note_count].tone_type = TONE_ENVELOPE;
        } else {
            notes[note_count].intensity = amp;
            notes[note_count].tone_type = TONE_NORMAL;
        }
        note_count++;
    }

    // Check for noise (independent frequency)
    if (noise_enabled) {
        double freq_hz = calc_noise_frequency(state);
        int noise_midi_note = freq_to_midi_note(freq_hz);
        if (noise_midi_note >= 0 && noise_midi_note != tone_midi_note) {
            // Only add if noise frequency is different from tone
            has_noise = true;
            notes[note_count].midi_note = noise_midi_note;
            notes[note_count].intensity = amp > 0 ? amp : 8;
            notes[note_count].tone_type = TONE_NOISE;
            note_count++;
        } else if (noise_midi_note >= 0) {
            // Noise has same MIDI note as tone - just mark it
            has_noise = true;
        }
    }

    // Draw register info (two lines, above piano)
    draw_register_info(hdc, LEFT_MARGIN, y_pos, state, channel);

    // Draw piano keyboard (below register info)
    // Piano displays C3 to B10 (8 octaves, width=840)
    // Layout: LEFT_MARGIN(20) + piano(840) + gap(15) + VU_L(35) + gap(5) + VU_R(35) + gap(15) + ENV(150) + gap(15) + SQUARE(70) + gap(10) + NOISE(70) = 1285
    int piano_x = LEFT_MARGIN;
    int piano_y = y_pos + 32;
    draw_piano_multi(hdc, piano_x, piano_y, notes, note_count);

    // Draw stereo VU meters (L and R) to the right of piano
    int vu_x = piano_x + PIANO_WIDTH + 15;

    // Draw Left VU meter
    draw_vu_meter(hdc, vu_x, piano_y, amp_left, "L");

    // Draw Right VU meter
    int vu_r_x = vu_x + VU_WIDTH + 5;
    draw_vu_meter(hdc, vu_r_x, piano_y, amp_right, "R");

    // Display amplitude values below VU meters
    char amp_text[64];
    sprintf(amp_text, "L:%02d R:%02d Tot:%02d", amp_left, amp_right, amp);
    SetTextColor(hdc, COLOR_TEXT_DIM);
    SetBkMode(hdc, TRANSPARENT);
    TextOut(hdc, vu_x, piano_y + VU_HEIGHT + 2, amp_text, strlen(amp_text));

    // Draw hardware envelope visualizations (only for channels that use envelopes)
    // Envelope 0 controls channels 1 and 4, Envelope 1 controls channels 2 and 5
    // But we'll show all envelope states for reference
    int env_x = vu_r_x + VU_WIDTH + 15;

    // Always show envelope 0 info on channel 1
    if (channel == 1) {
        draw_hardware_envelope(hdc, env_x, piano_y, state->envelope[0], 0);
    }
    // Always show envelope 1 info on channel 4
    if (channel == 4) {
        draw_hardware_envelope(hdc, env_x, piano_y, state->envelope[1], 1);
    }

    // Draw channel status boxes (NOISE and SQUARE) to the right of envelope
    // These are shown on all channels for reference
    int status_x = env_x + ENV_DISPLAY_WIDTH + 15;

    // Determine if this channel is using square wave (tone generator)
    bool square_active = ch_enabled && freq_val > 0;

    // Draw SQUARE status
    draw_status_box(hdc, status_x, piano_y, "SQUARE", square_active);

    // Draw NOISE status
    int noise_x = status_x + STATUS_BOX_WIDTH + 10;
    draw_status_box(hdc, noise_x, piano_y, "NOISE", noise_enabled);

    // Draw frequency info below piano - list all active sound sources
    char freq_text[300];
    freq_text[0] = '\0';
    bool first_entry = true;

    // Display tone/envelope frequency if present
    if (tone_midi_note >= 0) {
        double freq_hz = calc_frequency(freq_val, octave);
        char tone_info[100];
        sprintf(tone_info, "%.1f Hz", freq_hz);

        if (tone_midi_note < 128) {
            int note_idx = tone_midi_note % 12;
            int note_octave = tone_midi_note / 12;
            sprintf(tone_info + strlen(tone_info), " (%s%d)", note_names[note_idx], note_octave);
        }

        // Add type labels for what's active
        strcat(tone_info, " [");
        bool added_type = false;
        if (has_normal) {
            strcat(tone_info, "TONE");
            added_type = true;
        }
        if (has_envelope) {
            if (added_type) strcat(tone_info, "+");
            strcat(tone_info, "ENV");
            added_type = true;
        }
        strcat(tone_info, "]");

        strcat(freq_text, tone_info);
        first_entry = false;
    }

    // Display noise frequency if present and different
    if (has_noise) {
        double noise_freq = calc_noise_frequency(state);
        int noise_midi = freq_to_midi_note(noise_freq);

        // Only show separate entry if noise frequency differs from tone
        if (noise_midi != tone_midi_note) {
            if (!first_entry) {
                strcat(freq_text, " | ");
            }

            char noise_info[80];
            sprintf(noise_info, "%.1f Hz", noise_freq);

            if (noise_midi >= 0 && noise_midi < 128) {
                int note_idx = noise_midi % 12;
                int note_octave = noise_midi / 12;
                sprintf(noise_info + strlen(noise_info), " (%s%d)", note_names[note_idx], note_octave);
            }

            strcat(noise_info, " [NOISE]");
            strcat(freq_text, noise_info);
            first_entry = false;
        } else if (tone_midi_note >= 0) {
            // Noise shares same note - add to existing label
            // Find the closing bracket and insert before it
            char* bracket = strrchr(freq_text, ']');
            if (bracket) {
                // Move content after bracket
                memmove(bracket + 7, bracket, strlen(bracket) + 1);
                memcpy(bracket, "+NOISE]", 7);
            }
        }
    }

    if (freq_text[0] == '\0') {
        strcpy(freq_text, "---- Hz");
    }

    SetTextColor(hdc, COLOR_TEXT_DIM);
    TextOut(hdc, piano_x, piano_y + PIANO_HEIGHT + 3, freq_text, strlen(freq_text));

    // Draw separator line
    HPEN line_pen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
    HPEN old_pen = (HPEN)SelectObject(hdc, line_pen);
    MoveToEx(hdc, LEFT_MARGIN, y_pos + CHANNEL_HEIGHT - 5, NULL);
    LineTo(hdc, WINDOW_WIDTH - LEFT_MARGIN, y_pos + CHANNEL_HEIGHT - 5);
    SelectObject(hdc, old_pen);
    DeleteObject(line_pen);
}

// Window procedure
static LRESULT CALLBACK viz_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Get slot ID from window user data
    int slot = (int)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CHAR: {
            // Forward all keyboard input to the main keyboard handler via queue
            char key = (char)wParam;
            key_queue_enqueue(key);
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Create double buffer
            HDC mem_dc = CreateCompatibleDC(hdc);
            HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, WINDOW_WIDTH, WINDOW_HEIGHT);
            HBITMAP old_bmp = (HBITMAP)SelectObject(mem_dc, mem_bmp);

            // Clear background
            HBRUSH bg_brush = CreateSolidBrush(COLOR_BG);
            RECT client_rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
            FillRect(mem_dc, &client_rect, bg_brush);
            DeleteObject(bg_brush);

            // Set text properties
            SetBkMode(mem_dc, TRANSPARENT);
            HFONT font = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Consolas");
            HFONT old_font = (HFONT)SelectObject(mem_dc, font);

            EnterCriticalSection(&g_viz_lock);

            // Draw title with slot number
            SetTextColor(mem_dc, RGB(255, 255, 255));
            char title[128];
            sprintf(title, "SAA1099 Visualizer - Slot %d (Original Chip Status)", slot);
            HFONT title_font = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
            SelectObject(mem_dc, title_font);
            TextOut(mem_dc, LEFT_MARGIN, 10, title, strlen(title));
            SelectObject(mem_dc, font);
            DeleteObject(title_font);

            // Draw chip status with all global registers
            char status[256];
            if (g_saa_states[slot].enabled) {
                sprintf(status, "CH_EN:0x%02X | NOISE_EN:0x%02X | NOISE_CTRL:0x%02X | OCT[0-2]:0x%02X,0x%02X,0x%02X | ENV:0x%02X,0x%02X",
                       g_saa_states[slot].channel_enable,
                       g_saa_states[slot].noise_enable,
                       g_saa_states[slot].noise_control,
                       g_saa_states[slot].octave[0],
                       g_saa_states[slot].octave[1],
                       g_saa_states[slot].octave[2],
                       g_saa_states[slot].envelope[0],
                       g_saa_states[slot].envelope[1]);
            } else {
                sprintf(status, "No Active SAA1099 Chip - Press 'v' in player to toggle visualization");
            }
            SetTextColor(mem_dc, COLOR_TEXT_DIM);
            TextOut(mem_dc, LEFT_MARGIN, 32, status, strlen(status));

            // Draw all 6 channels for this slot
            if (g_saa_states[slot].enabled) {
                for (int ch = 0; ch < 6; ch++) {
                    int y_pos = TOP_MARGIN + ch * CHANNEL_HEIGHT;
                    draw_channel(mem_dc, ch, y_pos, &g_saa_states[slot]);
                }
            }

            LeaveCriticalSection(&g_viz_lock);

            // Cleanup
            SelectObject(mem_dc, old_font);
            DeleteObject(font);

            // Blit to screen
            BitBlt(hdc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, mem_dc, 0, 0, SRCCOPY);

            SelectObject(mem_dc, old_bmp);
            DeleteObject(mem_bmp);
            DeleteDC(mem_dc);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_CLOSE:
            // Clear this slot's window handle
            g_hwnd[slot] = NULL;
            DestroyWindow(hwnd);

            // Check if all windows are closed
            if (g_hwnd[0] == NULL && g_hwnd[1] == NULL) {
                g_viz_running = false;
                PostQuitMessage(0);
            }
            return 0;

        case WM_DESTROY:
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// Visualization thread function
static DWORD WINAPI viz_thread_func(LPVOID param) {
    (void)param;

    // Register window class (only if not already registered)
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = viz_window_proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "SAA1099VizClass";

    if (!RegisterClassEx(&wc)) {
        // If registration fails, check if it's because class already exists
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return 1;  // Real error, exit
        }
        // Class already exists, continue
    }

    // Create windows for each enabled slot
    RECT window_rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME, FALSE);

    int window_offset_x = 0;
    for (int slot = 0; slot < 2; slot++) {
        if (g_saa_states[slot].enabled) {
            char window_title[64];
            sprintf(window_title, "SAA1099 Visualizer - Slot %d", slot);

            g_hwnd[slot] = CreateWindowEx(
                WS_EX_TOPMOST,  // Create as topmost window
                "SAA1099VizClass",
                window_title,
                WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
                100 + window_offset_x, 100,  // Offset each window
                window_rect.right - window_rect.left,
                window_rect.bottom - window_rect.top,
                NULL, NULL,
                GetModuleHandle(NULL),
                NULL
            );

            if (!g_hwnd[slot]) {
                continue;  // Skip if creation failed
            }

            // Set slot ID as window user data
            SetWindowLongPtr(g_hwnd[slot], GWLP_USERDATA, slot);

            ShowWindow(g_hwnd[slot], SW_SHOW);
            UpdateWindow(g_hwnd[slot]);

            window_offset_x += 50;  // Cascade windows
        }
    }

    // Message loop with periodic refresh
    MSG msg;
    while (g_viz_running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_viz_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Refresh all windows at ~30 FPS
        for (int slot = 0; slot < 2; slot++) {
            if (g_hwnd[slot]) {
                InvalidateRect(g_hwnd[slot], NULL, FALSE);
            }
        }
        Sleep(33);
    }

    return 0;
}

// Public API implementations
extern "C" {

void saa1099_viz_init(void) {
    if (g_viz_initialized) return;

    InitializeCriticalSection(&g_viz_lock);
    memset(g_saa_states, 0, sizeof(g_saa_states));
    g_viz_initialized = true;
}

void saa1099_viz_cleanup(void) {
    if (!g_viz_initialized) return;

    // Ensure visualization is stopped first
    saa1099_viz_stop();

    // Now safe to delete critical section
    DeleteCriticalSection(&g_viz_lock);
    g_viz_initialized = false;
}

void saa1099_viz_start(void) {
    if (!g_viz_initialized || g_viz_running) return;

    g_viz_running = true;
    g_viz_thread = CreateThread(NULL, 0, viz_thread_func, NULL, 0, NULL);
}

void saa1099_viz_stop(void) {
    if (!g_viz_running) return;

    // Signal thread to stop
    g_viz_running = false;

    // Close all windows if they exist (this will trigger WM_QUIT in message loop)
    for (int slot = 0; slot < 2; slot++) {
        if (g_hwnd[slot]) {
            SendMessage(g_hwnd[slot], WM_CLOSE, 0, 0);
        }
    }

    // Wait for thread to finish
    if (g_viz_thread) {
        WaitForSingleObject(g_viz_thread, 5000);
        CloseHandle(g_viz_thread);
        g_viz_thread = NULL;
    }

    // Clear all window handles
    g_hwnd[0] = NULL;
    g_hwnd[1] = NULL;
}

void saa1099_viz_update_state(uint8_t slot, uint8_t reg, uint8_t data) {
    if (slot >= 2 || !g_viz_initialized) return;

    EnterCriticalSection(&g_viz_lock);

    saa1099_state_t* state = &g_saa_states[slot];
    state->last_reg = reg;
    state->last_data = data;

    // Update appropriate register
    if (reg <= 0x05) {
        state->amplitude[reg] = data;  // Save full 8 bits (includes L/R stereo in bits 7-6)
    } else if (reg >= 0x08 && reg <= 0x0D) {
        state->frequency[reg - 0x08] = data;
    } else if (reg >= 0x10 && reg <= 0x12) {
        state->octave[reg - 0x10] = data;
    } else if (reg == 0x14) {
        state->channel_enable = data;
    } else if (reg == 0x15) {
        state->noise_enable = data;
    } else if (reg == 0x16) {
        state->noise_control = data;
    } else if (reg == 0x18 || reg == 0x19) {
        state->envelope[reg - 0x18] = data;
    }

    LeaveCriticalSection(&g_viz_lock);
}

void saa1099_viz_set_slot_enabled(uint8_t slot, bool enabled) {
    if (slot >= 2 || !g_viz_initialized) return;

    EnterCriticalSection(&g_viz_lock);
    g_saa_states[slot].enabled = enabled;
    LeaveCriticalSection(&g_viz_lock);
}

bool saa1099_viz_is_slot_visible(uint8_t slot) {
    if (slot >= 2) return false;
    return g_hwnd[slot] != NULL;
}

} // extern "C"
