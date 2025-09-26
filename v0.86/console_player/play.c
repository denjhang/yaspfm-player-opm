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

void update_ui(uint32_t total_samples, const char* song_name, bool paused, bool random) {
    extern volatile int g_flush_mode;
    extern volatile double g_speed_multiplier;
    extern chip_config_t g_chip_config[CHIP_TYPE_COUNT];

    if (!g_ui_initialized) {
        init_ui();
    }

    // --- Static Part (drawn once) ---
    print_at(0, 0, "YASP - Yet Another Sound Player");
    print_at(0, 1, "--------------------------------------------------");
    print_at(0, 7, "--------------------------------------------------");
    print_at(0, 8, "[N] Next | [B] Prev | [P] Pause | [R] Random | [F] Browser | [+] Speed Up | [-] Speed Down | [Q] Quit");
    print_at(0, 9, "--------------------------------------------------");

    // --- Dynamic Part ---
    const char *base_name = strrchr(song_name, '/');
    if (base_name == NULL) base_name = strrchr(song_name, '\\');
    base_name = (base_name == NULL) ? song_name : base_name + 1;
    
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "Now Playing  : %s", base_name);
    clear_line(2); print_at(0, 2, buffer);

    if (total_samples > 0) {
        int total_sec = total_samples / 44100;
        snprintf(buffer, sizeof(buffer), "Total Time   : %02d:%02d", total_sec / 60, total_sec % 60);
        clear_line(3); print_at(0, 3, buffer);
    } else {
        clear_line(3);
    }

    const char* slot0_name = "NONE";
    const char* slot1_name = "NONE";
    for (int i = 1; i < CHIP_TYPE_COUNT; i++) {
        if (g_chip_config[i].slot == 0) slot0_name = chip_type_to_string((chip_type_t)i);
        if (g_chip_config[i].slot == 1) slot1_name = chip_type_to_string((chip_type_t)i);
    }
    snprintf(buffer, sizeof(buffer), "Slot 0: %s | Slot 1: %s", slot0_name, slot1_name);
    clear_line(4); print_at(0, 4, buffer);

    const char* flush_mode_str = (g_flush_mode == 1) ? "Register-Level" : "Command-Level";
    snprintf(buffer, sizeof(buffer), "Flush Mode (1,2): %s", flush_mode_str);
    clear_line(5); print_at(0, 5, buffer);

    snprintf(buffer, sizeof(buffer), "Timer (3-7): %s", get_timer_mode_string());
    clear_line(6); print_at(0, 6, buffer);

    const char* status_str = paused ? "Paused" : "Playing...";
    const char* random_str = random ? "On" : "Off";
    snprintf(buffer, sizeof(buffer), "Status: %s | Random: %s | Speed: %.2fx", status_str, random_str, g_speed_multiplier);
    clear_line(10); print_at(0, 10, buffer);

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

bool play_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        logging(LOG_ERROR, "fopen failed for %s", filename);
        return false;
    }

    bool result = false;
    enum filetype_t type = get_filetype(filename);

    // --- Update total samples for UI ---
    if (type == FILETYPE_VGM) {
        struct vgm_header_t header;
        if (vgm_parse_header(fp, &header)) {
            g_current_song_total_samples = header.total_samples;
            fseek(fp, 0, SEEK_SET); // Rewind after reading header
        }
    } else if (type == FILETYPE_S98) {
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

    switch (type) {
        case FILETYPE_VGM:
            result = vgm_play(fp, filename);
            break;
        case FILETYPE_S98: {
            S98 s98 = {0};
            if (s98_load(&s98, fp)) {
                result = s98_play(&s98, filename);
                s98_release(&s98);
            } else {
                logging(LOG_ERROR, "Failed to load S98 file: %s", filename);
            }
            break;
        }
        default:
            logging(LOG_ERROR, "Unsupported file type for: %s", filename);
            break;
    }

    fclose(fp);
    return result;
}
