#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include "spfm.h"
#include "play.h"
#include "error.h"
#include "chiptype.h"
#include "util.h"

#define INI_IMPLEMENTATION
#include "ini.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <mmsystem.h> // For timeBeginPeriod
#include <avrt.h>     // For AvSetMmThreadCharacteristics
#else
#include <pthread.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#endif

#define MAX_PLAYLIST_SIZE 512
#define MAX_FILENAME_LEN 256
#define CONFIG_FILENAME "config.ini"

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

// --- Configuration Struct ---
typedef struct {
    int device_index;
    char slot0_chip[50];
    char slot1_chip[50];
    double speed_multiplier;
} configuration;

// --- Function Prototypes ---
void scan_music_directory();
int get_next_song_index();
static int config_handler(void* user, const char* section, const char* name, const char* value, int lineno);
void save_configuration(int dev_idx, const char* slot0, const char* slot1, double speed);


#ifdef _WIN32
DWORD WINAPI keyboard_thread_func(LPVOID lpParam);
#else
void* keyboard_thread_func(void* arg);
void set_nonblocking_terminal();
void restore_terminal();
#endif


void scan_directory_recursive(const char *basePath) {
    char path[1000];
    struct dirent *dp;
    DIR *dir = opendir(basePath);

    if (!dir)
        return;

    while ((dp = readdir(dir)) != NULL && playlist_size < MAX_PLAYLIST_SIZE) {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
            strcpy(path, basePath);
            strcat(path, "/");
            strcat(path, dp->d_name);

            const char *ext = strrchr(dp->d_name, '.');
            if (ext && (!strcmp(ext, ".vgm") || !strcmp(ext, ".vgz") || !strcmp(ext, ".s98"))) {
                snprintf(playlist[playlist_size], MAX_FILENAME_LEN, "%s", path);
                playlist_size++;
            }
            scan_directory_recursive(path);
        }
    }
    closedir(dir);
}

void scan_music_directory() {
    scan_directory_recursive(".");
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
    } else {
        return 0;  // unknown section/name, error
    }
    return 1;
}

void save_configuration(int dev_idx, const char* slot0, const char* slot1, double speed) {
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
    fclose(file);
    printf("Configuration saved to %s\n", CONFIG_FILENAME);
}

// Platform-specific single character input
#ifdef _WIN32
int get_single_char() {
    return _getch();
}
#else
int get_single_char() {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif


#ifdef _WIN32
DWORD WINAPI keyboard_thread_func(LPVOID lpParam) {
    (void)lpParam;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    int c;
    while (!g_quit_flag) {
        if (_kbhit()) {
            c = _getch();
            switch (c) {
                case 'q': g_is_playing = false; g_quit_flag = true; break;
                case 'p': g_is_paused = !g_is_paused; break;
                case 'n': g_is_playing = false; g_next_track_flag = true; break;
                case 'b': g_is_playing = false; g_prev_track_flag = true; break;
                case 'r': g_random_mode = !g_random_mode; printf("Random mode %s\n", g_random_mode ? "ON" : "OFF"); break;
                case '+': 
                    g_speed_multiplier += 0.01; 
                    printf("Speed: %.2fx\n", g_speed_multiplier); 
                    // Quick save on change
                    {
                        const char* s0 = "NONE"; const char* s1 = "NONE";
                        for(int i=1; i < CHIP_TYPE_COUNT; ++i) {
                            if(g_chip_config[i].slot == 0) s0 = chip_type_to_string((chip_type_t)i);
                            if(g_chip_config[i].slot == 1) s1 = chip_type_to_string((chip_type_t)i);
                        }
                        save_configuration(spfm_get_selected_device_index(), s0, s1, g_speed_multiplier);
                    }
                    break;
                case '-': 
                    g_speed_multiplier -= 0.01; 
                    if (g_speed_multiplier < 0.01) g_speed_multiplier = 0.01; 
                    printf("Speed: %.2fx\n", g_speed_multiplier); 
                    // Quick save on change
                    {
                        const char* s0 = "NONE"; const char* s1 = "NONE";
                        for(int i=1; i < CHIP_TYPE_COUNT; ++i) {
                            if(g_chip_config[i].slot == 0) s0 = chip_type_to_string((chip_type_t)i);
                            if(g_chip_config[i].slot == 1) s1 = chip_type_to_string((chip_type_t)i);
                        }
                        save_configuration(spfm_get_selected_device_index(), s0, s1, g_speed_multiplier);
                    }
                    break;
                case '1': g_flush_mode = 1; printf("\nFlush Mode: Register-Level\n"); break;
                case '2': g_flush_mode = 2; printf("\nFlush Mode: Command-Level\n"); break;
            }
        }
        Sleep(50); // Prevent busy-waiting
    }
    return 0;
}
#else
struct termios oldt, newt;

void set_nonblocking_terminal() {
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, 0);
}

void* keyboard_thread_func(void* arg) {
    (void)arg;
    char c;
    set_nonblocking_terminal();
    while (!g_quit_flag) {
        if (read(STDIN_FILENO, &c, 1) > 0) {
            switch (c) {
                case 'q': g_is_playing = false; g_quit_flag = true; break;
                case 'p': g_is_paused = !g_is_paused; break;
                case 'n': g_is_playing = false; g_next_track_flag = true; break;
                case 'b': g_is_playing = false; g_prev_track_flag = true; break;
                case 'r': g_random_mode = !g_random_mode; printf("Random mode %s\n", g_random_mode ? "ON" : "OFF"); break;
                case '+': g_speed_multiplier += 0.05; printf("Speed: %.2fx\n", g_speed_multiplier); break;
                case '-': g_speed_multiplier -= 0.05; if (g_speed_multiplier < 0.05) g_speed_multiplier = 0.05; printf("Speed: %.2fx\n", g_speed_multiplier); break;
            }
        }
        usleep(50000); // 50ms
    }
    restore_terminal();
    return NULL;
}
#endif

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    srand(time(NULL));

#ifdef _WIN32
    // Request a 1ms timer resolution for the application.
    timeBeginPeriod(1);
    
    // Elevate thread priority for stable multimedia playback
    DWORD mmcss_task_index = 0;
    HANDLE h_mmcss = AvSetMmThreadCharacteristics("Pro Audio", &mmcss_task_index);
    if (h_mmcss == NULL) {
        logging(WARN, "Failed to enable MMCSS for thread, falling back to SetThreadPriority.");
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    }

    // Bind to a single core to prevent context switching overhead
    DWORD_PTR thread_affinity_mask = 1; // Bind to Core 0
    if (!SetThreadAffinityMask(GetCurrentThread(), thread_affinity_mask)) {
        logging(WARN, "Failed to set thread affinity mask.");
    }
#endif

    yasp_timer_init();

    FT_STATUS ftStatus;
    DWORD numDevs;
    int selected_dev_idx = -1;
    
    configuration config;
    config.device_index = -1;
    strcpy(config.slot0_chip, "NONE");
    strcpy(config.slot1_chip, "NONE");
    config.speed_multiplier = 1.0; // Default speed

    if (ini_parse(CONFIG_FILENAME, config_handler, &config) < 0) {
        printf("Can't load '%s', using defaults.\n", CONFIG_FILENAME);
    }
    g_speed_multiplier = config.speed_multiplier;

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

    // Chip configuration
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

    // Save current configuration
    const char* slot0_name = "NONE";
    const char* slot1_name = "NONE";
    for(int i=1; i < CHIP_TYPE_COUNT; ++i) {
        if(g_chip_config[i].slot == 0) slot0_name = chip_type_to_string((chip_type_t)i);
        if(g_chip_config[i].slot == 1) slot1_name = chip_type_to_string((chip_type_t)i);
    }
    save_configuration(selected_dev_idx, slot0_name, slot1_name, g_speed_multiplier);

    spfm_init_chips();

    scan_music_directory();
    if (playlist_size == 0) {
        logging(LOG_ERROR, "No .vgm or .s98 files found in 'music' directory.\n");
        return 1;
    }

#ifdef _WIN32
    HANDLE hThread = CreateThread(NULL, 0, keyboard_thread_func, NULL, 0, NULL);
#else
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, keyboard_thread_func, NULL);
#endif

    while (!g_quit_flag) {
        if (g_next_track_flag) {
            current_song_index = get_next_song_index();
            g_next_track_flag = false;
        }
        if (g_prev_track_flag) {
            current_song_index = (current_song_index - 1 + playlist_size) % playlist_size;
            g_prev_track_flag = false;
        }

        if (g_is_paused) {
            printf("Status: Paused... Press 'P' to resume.\n");
            while (g_is_paused && !g_quit_flag && !g_next_track_flag && !g_prev_track_flag) {
                Sleep(100);
            }
        }
        
        // Reset chips before playing each song
        spfm_chip_reset();
        
        g_is_playing = true;
        play_file(playlist[current_song_index]);
        g_is_playing = false; // Mark as not playing after file finishes or is skipped
        
        // If the song finished naturally (not skipped or quit), move to the next one
        if (!g_quit_flag && !g_next_track_flag && !g_prev_track_flag) {
            current_song_index = get_next_song_index();
        }
    }

    g_is_playing = false;

#ifdef _WIN32
    if (hThread != NULL) {
        WaitForSingleObject(hThread, 500);
        CloseHandle(hThread);
    }
#else
    pthread_join(thread_id, NULL);
#endif

    spfm_chip_reset(); // Reset hardware on exit
    spfm_cleanup();
    printf("\nPlayback finished.\n");

    yasp_timer_release();

#ifdef _WIN32
    // Release MMCSS and timer resolution
    if (h_mmcss != NULL) {
        AvRevertMmThreadCharacteristics(h_mmcss);
    }
    timeEndPeriod(1);
#endif

    return 0;
}
