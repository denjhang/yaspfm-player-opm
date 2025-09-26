#include "play.h"
#include "error.h"
#include "s98.h"
#include "vgm.h"
#include "chiptype.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

// This function is now designed to be called once at the start of a song.
extern int g_current_song_total_samples;

void update_ui(int current_samples, int total_samples, const char* song_name, bool paused, bool random) {
    (void)current_samples;
    (void)paused;
    (void)random;
    extern volatile int g_flush_mode;
    extern volatile int g_timer_mode;
    extern chip_config_t g_chip_config[CHIP_TYPE_COUNT];

    if (total_samples > 0) {
        g_current_song_total_samples = total_samples;
    }
    int total_sec = (g_current_song_total_samples > 0) ? g_current_song_total_samples / 44100 : 0;

    const char *base_name = strrchr(song_name, '/');
    if (base_name == NULL) {
        base_name = strrchr(song_name, '\\');
    }
    base_name = (base_name == NULL) ? song_name : base_name + 1;

    // Clear the screen once
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif

    printf("YASP - Yet Another Sound Player\n");
    printf("--------------------------------------------------\n");
    printf("Now Playing  : %s\n", base_name);
    if (total_sec > 0) {
        printf("Total Time   : %02d:%02d\n", total_sec / 60, total_sec % 60);
    }

    // Chip info
    printf("Slot 0: %s | Slot 1: %s\n",
           g_chip_config[1].slot == 0 ? chip_type_to_string(g_chip_config[1].type) : "NONE",
           g_chip_config[2].slot == 1 ? chip_type_to_string(g_chip_config[2].type) : "NONE");


    // Refresh modes
    const char* flush_mode_str = (g_flush_mode == 1) ? "Register-Level" : "Command-Level";
    printf("Flush Mode (1,2): %s\n", flush_mode_str);

    const char* timer_mode_str = "Unknown";
    switch(g_timer_mode) {
        case 0: timer_mode_str = "Default Sleep"; break;
        case 1: timer_mode_str = "Hybrid Sleep (10us)"; break;
        case 2: timer_mode_str = "Multimedia Timer (timeSetEvent)"; break;
        case 3: timer_mode_str = "Improved Default Sleep"; break;
        case 4: timer_mode_str = "Improved Hybrid Sleep"; break;
        case 5: timer_mode_str = "Improved Multimedia Timer"; break;
        case 6: timer_mode_str = "Experimental Timer"; break;
    }
    printf("Timer Mode (3-9): %s\n", timer_mode_str);


    printf("--------------------------------------------------\n");
    printf("[N] Next | [B] Prev | [P] Pause | [R] Random | [+] Speed Up | [-] Speed Down | [Q] Quit\n\n");
    printf("Status: Playing...\n");
    fflush(stdout);
}

static enum filetype_t get_filetype(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return FILETYPE_UNKNOWN;
    if (strcasecmp(ext, ".vgm") == 0) return FILETYPE_VGM;
    if (strcasecmp(ext, ".s98") == 0) return FILETYPE_S98;
    return FILETYPE_UNKNOWN;
}

bool play_file(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        logging(LOG_ERROR, "fopen failed for %s", filename);
        return false;
    }

    bool result = false;
    enum filetype_t type = get_filetype(filename);

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
