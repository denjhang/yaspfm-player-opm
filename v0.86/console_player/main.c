#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include "spfm.h"
#include "play.h"
#include "error.h"
#include "chiptype.h"
#include "util.h"
#include "browser.h"

#define INI_IMPLEMENTATION
#include "ini.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <mmsystem.h> // For timeBeginPeriod
#else
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#endif

#define MAX_PLAYLIST_SIZE 512
#define CONFIG_FILENAME "config.ini"

// --- Application State ---
typedef enum {
    MODE_PLAYER,
    MODE_BROWSER
} app_mode_t;

// --- Global State ---
char playlist[MAX_PLAYLIST_SIZE][MAX_FILENAME_LEN];
int playlist_size = 0;
int current_song_index = 0;

volatile bool g_is_playing = false;
volatile bool g_quit_flag = false;
volatile bool g_is_paused = false;
volatile bool g_next_track_flag = false;
volatile bool g_prev_track_flag = false;
volatile bool g_random_mode = false;
volatile double g_speed_multiplier = 1.0;
volatile int g_flush_mode = 2; // 1: Register-level, 2: Command-level (default)
volatile int g_timer_mode = 0; // 0: Default, 1: Hybrid Sleep, 2: Multimedia Timer
volatile bool g_ui_refresh_request = false;
char g_current_song_name[MAX_FILENAME_LEN] = "None";
int g_current_song_total_samples = 0;
volatile app_mode_t g_app_mode = MODE_PLAYER;
volatile bool g_new_song_request = false;
char g_requested_song_path[MAX_FILENAME_LEN];
volatile bool g_stop_current_song = false;
char g_info_message[256] = {0};

#ifdef _WIN32
CRITICAL_SECTION g_playlist_lock;
#else
pthread_mutex_t g_playlist_lock;
#endif


// --- Configuration Struct ---
typedef struct {
    int device_index;
    char slot0_chip[50];
    char slot1_chip[50];
    double speed_multiplier;
    int flush_mode;
    int timer_mode;
    char last_file[MAX_FILENAME_LEN];
} configuration;

// --- Function Prototypes ---
void scan_music_directory(const char* path);
int get_next_song_index();
static int config_handler(void* user, const char* section, const char* name, const char* value, int lineno);
void save_configuration(int dev_idx, const char* slot0, const char* slot1, double speed, int flush_mode, int timer_mode, const char* last_file);

#ifdef _WIN32
DWORD WINAPI keyboard_thread_func(LPVOID lpParam);
DWORD WINAPI player_thread_func(LPVOID lpParam);
#else
void* keyboard_thread_func(void* arg);
void* player_thread_func(void* arg);
#endif


void scan_directory_recursive(const char *basePath, bool clear_playlist) {
    if (clear_playlist) {
        playlist_size = 0;
    }

    char path[1000];
    struct dirent *dp;
    DIR *dir = opendir(basePath);

    if (!dir)
        return;

    while ((dp = readdir(dir)) != NULL && playlist_size < MAX_PLAYLIST_SIZE) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            snprintf(path, sizeof(path), "%s/%s", basePath, dp->d_name);

            struct stat st;
            if (stat(path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    scan_directory_recursive(path, false);
                } else {
                    const char *ext = strrchr(dp->d_name, '.');
                    if (ext && (strcasecmp(ext, ".vgm") == 0 || strcasecmp(ext, ".vgz") == 0 || strcasecmp(ext, ".s98") == 0)) {
                        snprintf(playlist[playlist_size], MAX_FILENAME_LEN, "%s", path);
                        playlist_size++;
                    }
                }
            }
        }
    }
    closedir(dir);
}

void scan_music_directory(const char* path) {
    scan_directory_recursive(path, true);
}

int get_next_song_index() {
    if (g_random_mode) {
        if (playlist_size > 1) {
            int next_index;
            do {
                next_index = rand() % playlist_size;
            } while (next_index == current_song_index);
            return next_index;
        }
    }
    return (current_song_index + 1) % playlist_size;
}

static int config_handler(void* user, const char* section, const char* name, const char* value, int lineno) {
    (void)lineno; // Unused parameter
    configuration* pconfig = (configuration*)user;
    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("device", "index")) {
        pconfig->device_index = atoi(value);
    } else if (MATCH("chips", "slot0")) {
        strncpy(pconfig->slot0_chip, value, sizeof(pconfig->slot0_chip) - 1);
    } else if (MATCH("chips", "slot1")) {
        strncpy(pconfig->slot1_chip, value, sizeof(pconfig->slot1_chip) - 1);
    } else if (MATCH("playback", "speed")) {
        pconfig->speed_multiplier = atof(value);
    } else if (MATCH("playback", "flush_mode")) {
        pconfig->flush_mode = atoi(value);
    } else if (MATCH("playback", "timer_mode")) {
        pconfig->timer_mode = atoi(value);
    } else if (MATCH("playback", "last_file")) {
        strncpy(pconfig->last_file, value, sizeof(pconfig->last_file) - 1);
    } else {
        return 0;  // unknown section/name, error
    }
    return 1;
}

void save_configuration(int dev_idx, const char* slot0, const char* slot1, double speed, int flush_mode, int timer_mode, const char* last_file) {
    FILE* file = fopen(CONFIG_FILENAME, "w");
    if (!file) {
        logging(LOG_ERROR, "Could not open %s for writing.\n", CONFIG_FILENAME);
        return;
    }
    fprintf(file, "[device]\n");
    fprintf(file, "index = %d\n", dev_idx);
    fprintf(file, "\n[chips]\n");
    fprintf(file, "slot0 = %s\n", slot0 ? slot0 : "NONE");
    fprintf(file, "slot1 = %s\n", slot1 ? slot1 : "NONE");
    fprintf(file, "\n[playback]\n");
    fprintf(file, "speed = %.2f\n", speed);
    fprintf(file, "flush_mode = %d\n", flush_mode);
    fprintf(file, "timer_mode = %d\n", timer_mode);
    if (last_file) {
        fprintf(file, "last_file = %s\n", last_file);
    }
    fclose(file);
}

#ifdef _WIN32
DWORD WINAPI keyboard_thread_func(LPVOID lpParam) {
    (void)lpParam;
    int c;
    while (!g_quit_flag) {
        if (_kbhit()) {
            c = _getch();
            if (c == 0 || c == 224) { // Arrow keys on Windows
                c = _getch();
            }
            
            EnterCriticalSection(&g_playlist_lock);
            switch (c) {
                case 'q': g_quit_flag = true; break;
                case 'p': g_is_paused = !g_is_paused; g_ui_refresh_request = true; break;
                case 'n': g_next_track_flag = true; break;
                case 'b': g_prev_track_flag = true; break;
                case 'r': g_random_mode = !g_random_mode; g_ui_refresh_request = true; break;
                case 'f': g_app_mode = (g_app_mode == MODE_PLAYER) ? MODE_BROWSER : MODE_PLAYER; g_ui_refresh_request = true; break;
                case '+': 
                    g_speed_multiplier += 0.01; 
                    g_ui_refresh_request = true;
                    break;
                case '-': 
                    g_speed_multiplier -= 0.01; 
                    if (g_speed_multiplier < 0.01) g_speed_multiplier = 0.01; 
                    g_ui_refresh_request = true;
                    break;
                case '1': g_flush_mode = 1; g_ui_refresh_request = true; break;
                case '2': g_flush_mode = 2; g_ui_refresh_request = true; break;
                case '3': g_timer_mode = 0; g_ui_refresh_request = true; break; // High-Precision Sleep
                case '4': g_timer_mode = 1; g_ui_refresh_request = true; break; // Hybrid Sleep
                case '5': g_timer_mode = 2; g_ui_refresh_request = true; break; // Multimedia Timer
                case '6': g_timer_mode = 3; g_ui_refresh_request = true; break; // VGMPlay Mode
                case '7': g_timer_mode = 7; g_ui_refresh_request = true; break; // Optimized VGMPlay Mode
            }
            LeaveCriticalSection(&g_playlist_lock);
        }
        Sleep(50); // Prevent busy-waiting
    }
    return 0;
}

DWORD WINAPI player_thread_func(LPVOID lpParam) {
    (void)lpParam;

    while (!g_quit_flag) {
        bool should_play = false;
        bool new_request = false;
        bool next_track = false;
        bool prev_track = false;

        EnterCriticalSection(&g_playlist_lock);
        new_request = g_new_song_request;
        next_track = g_next_track_flag;
        prev_track = g_prev_track_flag;
        should_play = g_is_playing;
        LeaveCriticalSection(&g_playlist_lock);

        if (new_request) {
            EnterCriticalSection(&g_playlist_lock);
            g_stop_current_song = true;
            LeaveCriticalSection(&g_playlist_lock);
            
            Sleep(50); // Give time for the playing song to stop

            EnterCriticalSection(&g_playlist_lock);
            char song_path_copy[MAX_FILENAME_LEN];
            strncpy(song_path_copy, g_requested_song_path, MAX_FILENAME_LEN);
            
            char *last_slash = strrchr(song_path_copy, '/');
            if (!last_slash) last_slash = strrchr(song_path_copy, '\\');
            if (last_slash) {
                *last_slash = '\0';
            } else {
                strcpy(song_path_copy, ".");
            }

            scan_music_directory(song_path_copy);

            current_song_index = -1;
            for (int i = 0; i < playlist_size; i++) {
                if (strcmp(playlist[i], g_requested_song_path) == 0) {
                    current_song_index = i;
                    break;
                }
            }
            if (current_song_index == -1) current_song_index = 0;

            g_new_song_request = false;
            g_is_playing = true;
            should_play = true;
            LeaveCriticalSection(&g_playlist_lock);
        }

        if (next_track) {
            EnterCriticalSection(&g_playlist_lock);
            current_song_index = get_next_song_index();
            g_next_track_flag = false;
            g_is_playing = true;
            should_play = true;
            LeaveCriticalSection(&g_playlist_lock);
        }
        if (prev_track) {
            EnterCriticalSection(&g_playlist_lock);
            current_song_index = (current_song_index - 1 + playlist_size) % playlist_size;
            g_prev_track_flag = false;
            g_is_playing = true;
            should_play = true;
            LeaveCriticalSection(&g_playlist_lock);
        }

        if (should_play) {
            EnterCriticalSection(&g_playlist_lock);
            g_stop_current_song = false;
            strncpy(g_current_song_name, playlist[current_song_index], MAX_FILENAME_LEN - 1);
            // g_ui_refresh_request is now set in play_file
            // g_ui_refresh_request = true; 
            
            // Save current song to config
            int dev_idx = spfm_get_dev_index();
            const char* slot0_name = "NONE";
            const char* slot1_name = "NONE";
            for(int i=1; i < CHIP_TYPE_COUNT; ++i) {
                if(g_chip_config[i].slot == 0) slot0_name = chip_type_to_string((chip_type_t)i);
                if(g_chip_config[i].slot == 1) slot1_name = chip_type_to_string((chip_type_t)i);
            }
            save_configuration(dev_idx, slot0_name, slot1_name, g_speed_multiplier, g_flush_mode, g_timer_mode, g_current_song_name);
            
            LeaveCriticalSection(&g_playlist_lock);

            spfm_chip_reset();
            play_file(playlist[current_song_index]);
            
            EnterCriticalSection(&g_playlist_lock);
            if (g_quit_flag) {
                g_is_playing = false;
            } else if (!g_next_track_flag && !g_prev_track_flag && !g_new_song_request) {
                 current_song_index = get_next_song_index();
                 g_is_playing = true;
            } else {
                 g_is_playing = false;
            }
            LeaveCriticalSection(&g_playlist_lock);
        }
        Sleep(100);
    }
    return 0;
}

#else
// POSIX implementations for threads
#endif

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    srand(time(NULL));

#ifdef _WIN32
    InitializeCriticalSection(&g_playlist_lock);
    timeBeginPeriod(1);
#endif

    yasp_timer_init();

    // ... (Device and Chip Configuration remains the same) ...
    FT_STATUS ftStatus;
    DWORD numDevs;
    int selected_dev_idx = -1;
    
    configuration config;
    config.device_index = -1;
    strcpy(config.slot0_chip, "NONE");
    strcpy(config.slot1_chip, "NONE");
    config.speed_multiplier = 1.0;
    config.flush_mode = 2;
    config.timer_mode = 0;
    config.last_file[0] = '\0';

    if (ini_parse(CONFIG_FILENAME, config_handler, &config) < 0) {
        printf("Can't load '%s', using defaults.\n", CONFIG_FILENAME);
    }
    g_speed_multiplier = config.speed_multiplier;
    g_flush_mode = config.flush_mode;
    g_timer_mode = config.timer_mode;

    ftStatus = FT_CreateDeviceInfoList(&numDevs);
    if (ftStatus != FT_OK) {
        logging(LOG_ERROR, "FT_CreateDeviceInfoList failed, error code: %d\n", (int)ftStatus);
        return 1;
    }

    if (numDevs == 0) {
        logging(LOG_ERROR, "No FTDI devices found.\n");
        return 1;
    }

    int spfm_dev_count = 0;
    int spfm_dev_indices[MAX_PLAYLIST_SIZE];

    printf("Scanning for SPFM devices...\n");
    for (DWORD i = 0; i < numDevs; i++) {
        char serial_number[16], description[64];
        if (FT_GetDeviceInfoDetail(i, NULL, NULL, NULL, NULL, serial_number, description, NULL) == FT_OK) {
            if (strstr(description, "SPFM") != NULL || strstr(description, "USB UART") != NULL) {
                 printf("Found SPFM compatible device at index %lu: %s, SN: %s\n", i, description, serial_number);
                 spfm_dev_indices[spfm_dev_count++] = i;
            }
        }
    }

    if (spfm_dev_count == 0) {
        logging(LOG_ERROR, "No SPFM devices found.\n");
        return 1;
    }

    if (spfm_dev_count > 1) {
        printf("\nAvailable SPFM devices:\n");
        for (int i = 0; i < spfm_dev_count; i++) {
            char serial_number[16], description[64];
            FT_GetDeviceInfoDetail(spfm_dev_indices[i], NULL, NULL, NULL, NULL, serial_number, description, NULL);
            printf("[%d] %s, SN: %s\n", i, description, serial_number);
        }

        printf("Select device: ");
        int choice_char = get_single_char();
        printf("%c\n", choice_char);
        int choice = choice_char - '0';

        if (choice < 0 || choice >= spfm_dev_count) {
            logging(LOG_ERROR, "Invalid device selection.\n");
            return 1;
        }
        selected_dev_idx = spfm_dev_indices[choice];
    } else {
        selected_dev_idx = spfm_dev_indices[0];
    }

    if (spfm_init(selected_dev_idx) != 0) {
        logging(LOG_ERROR, "Failed to initialize SPFM device.\n");
        return 1;
    }

    printf("\n--- Chip Configuration ---\n");
    int saved_slot0_idx = string_to_chip_type(config.slot0_chip);
    int saved_slot1_idx = string_to_chip_type(config.slot1_chip);

    for (int i = 0; i < CHIP_TYPE_COUNT; i++) {
        g_chip_config[i].type = (chip_type_t)i;
        g_chip_config[i].slot = 0xFF;
    }

    for (int slot = 0; slot < 2; slot++) {
        printf("\nSelect chip for slot %d (Enter to use saved: %s):\n", slot, (slot == 0) ? config.slot0_chip : config.slot1_chip);
        for (int i = 1; i < CHIP_TYPE_COUNT; i++) {
            if (i < 10) {
                printf("[%d] %s\n", i, chip_type_to_string((chip_type_t)i));
            } else {
                printf("[%c] %s\n", 'a' + (i - 10), chip_type_to_string((chip_type_t)i));
            }
        }
        printf("Enter chip choice (0 to skip): ");
        int choice_char = get_single_char();
        printf("%c\n", choice_char);

        int chip_choice = -1;
        if (choice_char == '\r' || choice_char == '\n') {
            chip_choice = (slot == 0) ? saved_slot0_idx : saved_slot1_idx;
        } else if (choice_char >= '1' && choice_char <= '9') {
            chip_choice = choice_char - '0';
        } else if (choice_char >= 'a' && choice_char <= 'z') {
            chip_choice = choice_char - 'a' + 10;
        } else if (choice_char == '0') {
            chip_choice = 0;
        }

        if (chip_choice > 0 && chip_choice < CHIP_TYPE_COUNT) {
            g_chip_config[chip_choice].slot = slot;
        } else {
            printf("Invalid choice or 0. Skipping slot.\n");
        }
    }

    const char* slot0_name = "NONE";
    const char* slot1_name = "NONE";
    for(int i=1; i < CHIP_TYPE_COUNT; ++i) {
        if(g_chip_config[i].slot == 0) slot0_name = chip_type_to_string((chip_type_t)i);
        if(g_chip_config[i].slot == 1) slot1_name = chip_type_to_string((chip_type_t)i);
    }
    save_configuration(selected_dev_idx, slot0_name, slot1_name, g_speed_multiplier, g_flush_mode, g_timer_mode, config.last_file[0] ? config.last_file : NULL);

    spfm_init_chips();

    if (config.last_file[0] != '\0') {
        char last_file_copy[MAX_FILENAME_LEN];
        strncpy(last_file_copy, config.last_file, MAX_FILENAME_LEN);
        char *last_slash = strrchr(last_file_copy, '/');
        if (!last_slash) last_slash = strrchr(last_file_copy, '\\');
        if (last_slash) {
            *last_slash = '\0';
            scan_music_directory(last_file_copy);
        } else {
            scan_music_directory(".");
        }

        for (int i = 0; i < playlist_size; i++) {
            if (strcmp(playlist[i], config.last_file) == 0) {
                current_song_index = i;
                break;
            }
        }
    } else {
        scan_music_directory(".");
    }
    if (playlist_size == 0) {
        logging(LOG_ERROR, "No .vgm or .s98 files found.\n");
        return 1;
    }
    
    g_is_playing = true; // Start playing the first song

#ifdef _WIN32
    HANDLE hPlayerThread = CreateThread(NULL, 0, player_thread_func, NULL, 0, NULL);
    HANDLE hKeyboardThread = CreateThread(NULL, 0, keyboard_thread_func, NULL, 0, NULL);
#else
    // POSIX thread creation
#endif

    // Main UI loop
    while (!g_quit_flag) {
        if (g_app_mode == MODE_PLAYER) {
            if (g_ui_refresh_request) {
                update_ui(g_current_song_total_samples, g_current_song_name, g_is_paused, g_random_mode);
                g_ui_refresh_request = false;
            }
        } else { // MODE_BROWSER
            const char* selected_file = file_browser(g_current_song_name);
            if (selected_file) {
                EnterCriticalSection(&g_playlist_lock);
                strncpy(g_requested_song_path, selected_file, MAX_FILENAME_LEN - 1);
                g_new_song_request = true;
                g_stop_current_song = true; // Force stop to play new song immediately
                LeaveCriticalSection(&g_playlist_lock);
            }
            g_app_mode = MODE_PLAYER;
            init_ui(); // Force full redraw
            g_ui_refresh_request = true; 
        }
        Sleep(100);
    }

#ifdef _WIN32
    if (hPlayerThread != NULL) { WaitForSingleObject(hPlayerThread, INFINITE); CloseHandle(hPlayerThread); }
    if (hKeyboardThread != NULL) { WaitForSingleObject(hKeyboardThread, INFINITE); CloseHandle(hKeyboardThread); }
#else
    // POSIX thread joining
#endif

    spfm_chip_reset();
    spfm_cleanup();
    printf("\nPlayback finished.\n");

#ifdef _WIN32
    timeEndPeriod(1);
    DeleteCriticalSection(&g_playlist_lock);
#endif

    return 0;
}
