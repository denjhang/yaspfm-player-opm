#include "play.h"
#include "error.h"
#include "s98.h"
#include "vgm.h"
#include "chiptype.h"
#include "util.h"
#include "spfm.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// --- Externs for UI from other modules ---
extern volatile double g_opn_lfo_amplitude;
extern bool g_opn_to_opm_conversion_enabled;
extern bool g_ay_to_opm_conversion_enabled;
extern bool g_sn_to_ay_conversion_enabled;
extern bool g_ws_to_opm_conversion_enabled;
extern volatile bool g_is_playing_from_cache;
extern chip_type_t g_vgm_chip_type;
extern chip_type_t g_original_vgm_chip_type;
extern vgm_header_t g_vgm_header;

// --- Globals for UI ---
static bool g_ui_initialized = false;
#ifdef _WIN32
static HANDLE g_h_console;
static CONSOLE_SCREEN_BUFFER_INFO g_console_info;
#endif

void init_ui() {
    #ifdef _WIN32
    g_h_console = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(g_h_console, &g_console_info);
    system("cls");
    #else
    system("clear");
    #endif
    g_ui_initialized = true;
}

void set_cursor_pos(int x, int y) {
    #ifdef _WIN32
    COORD coord = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(g_h_console, coord);
    #else
    printf("\033[%d;%dH", y + 1, x + 1);
    #endif
}

void clear_line(int y) {
    set_cursor_pos(0, y);
    #ifdef _WIN32
    DWORD count;
    FillConsoleOutputCharacter(g_h_console, ' ', g_console_info.dwSize.X, (COORD){0, (SHORT)y}, &count);
    #else
    printf("\033[K");
    #endif
}

void print_at(int x, int y, const char* text) {
    set_cursor_pos(x, y);
    printf("%s", text);
}

// Helper to safely convert wchar_t* to char* for display
static void safe_wcstombs(char* dest, size_t dest_size, const wchar_t* src) {
    if (src == NULL || src[0] == L'\0') {
        dest[0] = '\0';
        return;
    }
    size_t result = wcstombs(dest, src, dest_size);
    if (result == (size_t)-1) {
        // Conversion error, copy a placeholder
        snprintf(dest, dest_size, "[conv_err]");
    } else if (result >= dest_size) {
        // Output was truncated, ensure null termination
        dest[dest_size - 1] = '\0';
    }
}

const char* get_timer_mode_string() {
    extern volatile int g_timer_mode;
    switch (g_timer_mode) {
        case 0: return "H-Prec Sleep (Compensated)";
        case 1: return "Hybrid Sleep (Compensated)";
        case 2: return "MM-Timer (Compensated)";
        case 3: return "VGMPlay Mode";
        case 7: return "Optimized VGMPlay Mode";
        default: return "Unknown";
    }
}

void update_ui(uint32_t total_samples, const char* song_name, bool paused, int play_mode, ay_stereo_mode_t ay_stereo_mode, cache_mode_t cache_mode) {
    extern volatile int g_flush_mode;
    extern volatile double g_speed_multiplier;
    extern chip_config_t g_chip_config[CHIP_TYPE_COUNT];

    if (!g_ui_initialized) {
        init_ui();
    }

    // --- Static Part (drawn once) ---
    print_at(0, 0, "YASP - Yet Another Sound Player");
    print_at(0, 1, "--------------------------------------------------");
    print_at(0, 17, "--------------------------------------------------");
    print_at(0, 18, "[Up/Down] LFO Amp | [Left/Right] Loops | [N] Next | [B] Prev | [P] Pause | [S] Random");
    print_at(0, 19, "[R] Replay | [F] Browser | [+/-] Speed | [C] Cache Mode | [Tab] AY Stereo | [Q] Quit");
    print_at(0, 20, "--------------------------------------------------");

    // --- Dynamic Part ---
    char buffer[512];
    
    // --- GD3 Info ---
    char temp_str[256];

    // Track
    const wchar_t* track_str = g_vgm_header.track_name_en[0] ? g_vgm_header.track_name_en : g_vgm_header.track_name_jp;
    if (track_str[0]) {
        safe_wcstombs(temp_str, sizeof(temp_str), track_str);
        snprintf(buffer, sizeof(buffer), "Track: %s", temp_str);
    } else {
        const char *base_name = strrchr(song_name, '/');
        if (base_name == NULL) base_name = strrchr(song_name, '\\');
        base_name = (base_name == NULL) ? song_name : base_name + 1;
        snprintf(buffer, sizeof(buffer), "File: %s", base_name);
    }
    clear_line(2); print_at(0, 2, buffer);

    // Game
    const wchar_t* game_str = g_vgm_header.game_name_en[0] ? g_vgm_header.game_name_en : g_vgm_header.game_name_jp;
    if (game_str[0]) {
        safe_wcstombs(temp_str, sizeof(temp_str), game_str);
        snprintf(buffer, sizeof(buffer), "Game: %s", temp_str);
    } else {
        snprintf(buffer, sizeof(buffer), "Game: (none)");
    }
    clear_line(3); print_at(0, 3, buffer);

    // System
    const wchar_t* system_str = g_vgm_header.system_name_en[0] ? g_vgm_header.system_name_en : g_vgm_header.system_name_jp;
    if (system_str[0]) {
        safe_wcstombs(temp_str, sizeof(temp_str), system_str);
        snprintf(buffer, sizeof(buffer), "System: %s", temp_str);
    } else {
        snprintf(buffer, sizeof(buffer), "System: (none)");
    }
    clear_line(4); print_at(0, 4, buffer);

    // Author
    const wchar_t* author_str = g_vgm_header.author_en[0] ? g_vgm_header.author_en : g_vgm_header.author_jp;
    if (author_str[0]) {
        safe_wcstombs(temp_str, sizeof(temp_str), author_str);
        snprintf(buffer, sizeof(buffer), "Author: %s", temp_str);
    } else {
        snprintf(buffer, sizeof(buffer), "Author: (none)");
    }
    clear_line(5); print_at(0, 5, buffer);

    // Release Date
    if (g_vgm_header.release_date[0]) {
        safe_wcstombs(temp_str, sizeof(temp_str), g_vgm_header.release_date);
        snprintf(buffer, sizeof(buffer), "Date: %s", temp_str);
    } else {
        snprintf(buffer, sizeof(buffer), "Date: (none)");
    }
    clear_line(6); print_at(0, 6, buffer);

    // VGM Creator
    if (g_vgm_header.vgm_creator[0]) {
        safe_wcstombs(temp_str, sizeof(temp_str), g_vgm_header.vgm_creator);
        snprintf(buffer, sizeof(buffer), "VGM By: %s", temp_str);
    } else {
        snprintf(buffer, sizeof(buffer), "VGM By: (none)");
    }
    clear_line(7); print_at(0, 7, buffer);

    // --- Show VGM Chip Info ---
    uint32_t clock = 0;
    chip_type_t display_chip = g_is_playing_from_cache ? g_original_vgm_chip_type : g_vgm_chip_type;
    uint32_t default_clock = get_chip_default_clock(display_chip);

    if (display_chip == CHIP_TYPE_YM2612) clock = g_vgm_header.ym2612_clock;
    else if (display_chip == CHIP_TYPE_YM2203) clock = g_vgm_header.ym2203_clock;
    else if (display_chip == CHIP_TYPE_YM2608) clock = g_vgm_header.ym2608_clock;
    else if (display_chip == CHIP_TYPE_YM2151) clock = g_vgm_header.ym2151_clock;
    else if (display_chip == CHIP_TYPE_AY8910) clock = g_vgm_header.ay8910_clock;
    else if (display_chip == CHIP_TYPE_SN76489) clock = g_vgm_header.sn76489_clock;
    else if (display_chip == CHIP_TYPE_WSWAN) clock = g_vgm_header.wonderswan_clock;
    // Add other chips here

    if (clock == 0) {
        clock = default_clock;
    }

    // Check if all possible clocks are zero
    if (g_vgm_header.ym2612_clock == 0 && g_vgm_header.ym2151_clock == 0 &&
        g_vgm_header.ym2203_clock == 0 && g_vgm_header.ym2608_clock == 0 &&
        g_vgm_header.sn76489_clock == 0 && g_vgm_header.ay8910_clock == 0 &&
        g_vgm_header.ym2413_clock == 0 && g_vgm_header.wonderswan_clock == 0) {
        snprintf(buffer, sizeof(buffer), "VGM Chip: Invalid/Unsupported");
    } else {
        snprintf(buffer, sizeof(buffer), "VGM Chip: %s (%.2fMHz)", chip_type_to_string(display_chip), clock / 1000000.0);
    }
    clear_line(8); print_at(0, 8, buffer);

    const char* status_str = paused ? "Paused" : "Playing...";
    snprintf(buffer, sizeof(buffer), "Status: %s", status_str);
    clear_line(9); print_at(0, 9, buffer);

    extern volatile int g_vgm_loop_count;
    if (total_samples > 0) {
        uint32_t total_display_samples = total_samples;
        if (g_vgm_header.loop_samples > 0 && g_vgm_loop_count > 1) {
            total_display_samples += g_vgm_header.loop_samples * (g_vgm_loop_count - 1);
        }
        int total_sec = total_display_samples / 44100;
        snprintf(buffer, sizeof(buffer), "Total Time: %02d:%02d | Loops: %d", total_sec / 60, total_sec % 60, g_vgm_loop_count);
        clear_line(10); print_at(0, 10, buffer);
    } else {
        clear_line(10);
    }

    const char* flush_mode_str = (g_flush_mode == 1) ? "Register-Level" : "Command-Level";
    snprintf(buffer, sizeof(buffer), "Flush Mode (1,2): %s", flush_mode_str);
    clear_line(11); print_at(0, 11, buffer);

    snprintf(buffer, sizeof(buffer), "Timer (3-7): %s", get_timer_mode_string());
    clear_line(12); print_at(0, 12, buffer);

    const char* mode_str = (play_mode == 1) ? "Random" : "Sequential";
    snprintf(buffer, sizeof(buffer), "Mode: %s | Speed: %.2fx", mode_str, g_speed_multiplier);
    clear_line(13); print_at(0, 13, buffer);

    if (g_opn_to_opm_conversion_enabled) {
        snprintf(buffer, sizeof(buffer), "OPN LFO Amp: %.2f", g_opn_lfo_amplitude);
        clear_line(14); print_at(0, 14, buffer);
    } else if (g_ay_to_opm_conversion_enabled) {
        const char* cache_mode_str = (cache_mode == CACHE_MODE_UPDATE) ? "Update" : "Normal";
        snprintf(buffer, sizeof(buffer), "AY Stereo (Tab): %s | Cache (C): %s", ay_to_opm_get_stereo_mode_name(ay_stereo_mode), cache_mode_str);
        clear_line(14); print_at(0, 14, buffer);
    } else {
        clear_line(14);
    }

    const char* slot0_name = "NONE";
    const char* slot1_name = "NONE";
    for (int i = 1; i < CHIP_TYPE_COUNT; i++) {
        if (g_chip_config[i].slot == 0) slot0_name = chip_type_to_string((chip_type_t)i);
        if (g_chip_config[i].slot == 1) slot1_name = chip_type_to_string((chip_type_t)i);
    }
    snprintf(buffer, sizeof(buffer), "Slot 0: %s (%.2fMHz) | Slot 1: %s (%.2fMHz)", 
        slot0_name, get_chip_default_clock(get_chip_type_from_string(slot0_name)) / 1000000.0,
        slot1_name, get_chip_default_clock(get_chip_type_from_string(slot1_name)) / 1000000.0);
    clear_line(15); print_at(0, 15, buffer);

    if (g_is_playing_from_cache) {
        snprintf(buffer, sizeof(buffer), "Conversion: (Cached) %s -> YM2151", chip_type_to_string(g_original_vgm_chip_type));
        clear_line(16); print_at(0, 16, buffer);
    } else if (g_opn_to_opm_conversion_enabled || g_ay_to_opm_conversion_enabled || g_sn_to_ay_conversion_enabled || g_ws_to_opm_conversion_enabled) {
        uint32_t conv_clock = 0;
        if (g_original_vgm_chip_type == CHIP_TYPE_YM2612) conv_clock = g_vgm_header.ym2612_clock;
        else if (g_original_vgm_chip_type == CHIP_TYPE_YM2203) conv_clock = g_vgm_header.ym2203_clock;
        else if (g_original_vgm_chip_type == CHIP_TYPE_YM2608) conv_clock = g_vgm_header.ym2608_clock;
        else if (g_original_vgm_chip_type == CHIP_TYPE_AY8910) conv_clock = g_vgm_header.ay8910_clock;
        else if (g_original_vgm_chip_type == CHIP_TYPE_SN76489) conv_clock = g_vgm_header.sn76489_clock;
        else if (g_original_vgm_chip_type == CHIP_TYPE_WSWAN) conv_clock = g_vgm_header.wonderswan_clock;
        
        if (conv_clock == 0) {
            conv_clock = get_chip_default_clock(g_original_vgm_chip_type);
        }

        snprintf(buffer, sizeof(buffer), "Conversion: %s (%.2fMHz) -> YM2151 (%.2fMHz)", 
            chip_type_to_string(g_original_vgm_chip_type), 
            conv_clock / 1000000.0,
            get_chip_default_clock(CHIP_TYPE_YM2151) / 1000000.0);
        clear_line(16); print_at(0, 16, buffer);
    } else {
        snprintf(buffer, sizeof(buffer), "Conversion: Direct Play");
        clear_line(16); print_at(0, 16, buffer);
    }

    fflush(stdout);
}

static enum filetype_t get_filetype(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return FILETYPE_UNKNOWN;
    if (strcasecmp(ext, ".vgm") == 0) return FILETYPE_VGM;
    if (strcasecmp(ext, ".s98") == 0) return FILETYPE_S98;
    return FILETYPE_UNKNOWN;
}

extern volatile bool g_ui_refresh_request;
extern int g_current_song_total_samples;

bool play_file(const char *filename, bool force_reconvert) {
    logging(LOG_LEVEL_DEBUG, "Attempting to play file: %s", filename);

    char cache_filename[MAX_PATH_LEN] = {0};
    const char *base_name = strrchr(filename, '/');
    if (base_name == NULL) base_name = strrchr(filename, '\\');
    base_name = (base_name == NULL) ? filename : base_name + 1;

    // This is a potential cache name, vgm.c will decide if it's needed
    snprintf(cache_filename, sizeof(cache_filename), "console_player/cache/%s.opm.vgm", base_name);

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        logging(LOG_LEVEL_ERROR, "fopen failed for %s", filename);
        return false;
    }

    bool result = false;
    enum filetype_t type = get_filetype(filename);

    // --- Update total samples for UI ---
    if (type == FILETYPE_S98) {
        S98 s98 = {0};
        if (s98_load(&s98, fp)) {
            // Calculate total samples for S98
            uint32_t total_samples = 0;
            uint32_t pos = s98.offset_to_dump;
            while (pos < s98.size) {
                uint8_t op = s98.buffer[pos++];
                if (op == 0x00) { // SYNC
                    total_samples++;
                } else if (op == 0x01) { // SYNC(n)
                    uint8_t n = 1;
                    while (s98.buffer[pos] == 0x01) {
                        n++;
                        pos++;
                    }
                    total_samples += n;
                } else if (op >= 0x80) { // FM/SSG/etc
                    pos += 2;
                } else if (op == 0xFF) { // END
                    break;
                }
            }
            g_current_song_total_samples = (uint32_t)(((double)total_samples / 1000.0) * 44100.0);
            s98_release(&s98);
            fseek(fp, 0, SEEK_SET); // Rewind
        }
    } else {
        g_current_song_total_samples = 0;
    }

    g_ui_refresh_request = true; // Request UI refresh now that we have the total samples

    // For VGM, header parsing and sample count is handled inside vgm_play
    // to correctly deal with cached files.
    if (type != FILETYPE_VGM) {
        g_current_song_total_samples = 0; // Reset for non-VGM files initially
    }

    switch (type) {
        case FILETYPE_VGM: {
            logging(LOG_LEVEL_DEBUG, "Calling vgm_play for %s", filename);
            FILE* final_fp = vgm_play(fp, filename, cache_filename, force_reconvert);
            // vgm_play will close the original fp if it creates a cache.
            // It returns the handle that needs to be closed by the caller.
            if (final_fp) {
                fclose(final_fp);
            }
            result = true; // Assume success for now
            return result; // Return early to avoid double-closing fp
        }
        case FILETYPE_S98: {
            S98 s98 = {0};
            if (s98_load(&s98, fp)) {
                logging(LOG_LEVEL_DEBUG, "Calling s98_play for %s", filename);
                result = s98_play(&s98, filename);
                s98_release(&s98);
            } else {
                logging(LOG_LEVEL_ERROR, "Failed to load S98 file: %s", filename);
            }
            break;
        }
        default:
            logging(LOG_LEVEL_ERROR, "Unsupported file type for: %s", filename);
            break;
    }

    if (fp) fclose(fp);
    return result;
}
