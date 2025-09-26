#include "browser.h"
#include "util.h"
#include "play.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#endif

#define MAX_FILES 512
#define MAX_PATH_LEN 1024

typedef struct {
    char name[256];
    bool is_dir;
} FileEntry;

static FileEntry file_list[MAX_FILES];
static int file_count = 0;
static int selected_index = 0;
static char current_path[MAX_PATH_LEN] = ".";
static char selected_file_path[MAX_PATH_LEN];
extern char g_current_song_name[MAX_FILENAME_LEN];

#define PAGE_SIZE 10
static int current_page = 0;

int compare_entries(const void* a, const void* b) {
    const FileEntry* entryA = (const FileEntry*)a;
    const FileEntry* entryB = (const FileEntry*)b;
    if (entryA->is_dir != entryB->is_dir) {
        return entryB->is_dir - entryA->is_dir; // Directories first
    }
    return strcasecmp(entryA->name, entryB->name);
}

void populate_file_list() {
    file_count = 0;
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(current_path))) {
        return;
    }

    while ((entry = readdir(dir)) != NULL && file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0) continue;

        FileEntry fe;
        strncpy(fe.name, entry->d_name, sizeof(fe.name) - 1);
        
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry->d_name);
        
        #ifdef _WIN32
            DWORD attrib = GetFileAttributes(full_path);
            fe.is_dir = (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
        #else
            struct stat st;
            if (stat(full_path, &st) == 0) {
                fe.is_dir = S_ISDIR(st.st_mode);
            } else {
                fe.is_dir = false;
            }
        #endif
        
        const char *ext = strrchr(entry->d_name, '.');
        if (fe.is_dir || (ext && (strcasecmp(ext, ".vgm") == 0 || strcasecmp(ext, ".vgz") == 0 || strcasecmp(ext, ".s98") == 0))) {
            file_list[file_count++] = fe;
        }
    }
    closedir(dir);
    qsort(file_list, file_count, sizeof(FileEntry), compare_entries);
}

void display_file_list() {
    clear_screen();
    int total_pages = (file_count + PAGE_SIZE - 1) / PAGE_SIZE;
    if (total_pages == 0) total_pages = 1;
    if (current_page >= total_pages) current_page = total_pages - 1;

    printf("File Browser\n");
    printf("--------------------------------------------------\n");
    printf("Current Dir: %s\n", current_path);
    printf("Now Playing: %s\n", g_current_song_name);
    printf("--------------------------------------------------\n");

    int start_index = current_page * PAGE_SIZE;
    int end_index = start_index + PAGE_SIZE;
    if (end_index > file_count) end_index = file_count;

    for (int i = start_index; i < end_index; i++) {
        printf("%s %s%s\n", (i == selected_index) ? "->" : "  ", file_list[i].name, file_list[i].is_dir ? "/" : "");
    }

    // Fill the rest of the page with blank lines
    for (int i = end_index - start_index; i < PAGE_SIZE; i++) {
        printf("\n");
    }

    printf("--------------------------------------------------\n");
    printf("Page %d/%d | [Up/Down] Navigate | [Left/Right] Page | [Enter] Select | [F] Exit\n", current_page + 1, total_pages);
}

const char* file_browser(const char* current_song_path) {
    // Set initial path from current song
    if (current_song_path && strcmp(current_song_path, "None") != 0) {
        strncpy(current_path, current_song_path, sizeof(current_path) - 1);
        char* last_slash = strrchr(current_path, '/');
        if (!last_slash) last_slash = strrchr(current_path, '\\');
        if (last_slash) {
            *last_slash = '\0';
        } else {
            strcpy(current_path, ".");
        }
    }

    populate_file_list();
    display_file_list();

    int ch;
    while (1) {
        ch = get_single_char();
#ifdef _WIN32
        if (ch == 0 || ch == 224) { // Arrow keys on Windows send two codes
            ch = get_single_char();
        }
#endif
        switch (ch) {
            case 72: // Up Arrow
                selected_index = (selected_index - 1 + file_count) % file_count;
                current_page = selected_index / PAGE_SIZE;
                break;
            case 80: // Down Arrow
                selected_index = (selected_index + 1) % file_count;
                current_page = selected_index / PAGE_SIZE;
                break;
            case 75: // Left Arrow
                if (current_page > 0) {
                    current_page--;
                    selected_index = current_page * PAGE_SIZE;
                }
                break;
            case 77: // Right Arrow
                {
                    int total_pages = (file_count + PAGE_SIZE - 1) / PAGE_SIZE;
                    if (current_page < total_pages - 1) {
                        current_page++;
                        selected_index = current_page * PAGE_SIZE;
                    }
                }
                break;
            case 13: // Enter
                if (file_list[selected_index].is_dir) {
                    if (strcmp(file_list[selected_index].name, "..") == 0) {
                        // Go up
                        char* last_slash = strrchr(current_path, '/');
                        if (last_slash) {
                            *last_slash = '\0';
                        } else {
                            strcpy(current_path, ".");
                        }
                    } else {
                        // Go down
                        snprintf(current_path, sizeof(current_path), "%s/%s", current_path, file_list[selected_index].name);
                    }
                    selected_index = 0;
                    populate_file_list();
                } else {
                    // Play file
                    snprintf(selected_file_path, sizeof(selected_file_path), "%s/%s", current_path, file_list[selected_index].name);
                    return selected_file_path;
                }
                break;
            case 8: // Backspace
                {
                    char* last_slash = strrchr(current_path, '/');
                     if (last_slash != NULL && current_path[1] != '\0') { // don't go up from "." or "C:"
                        *last_slash = '\0';
                    } else {
                        // If already at root, do nothing or go to drive list on Windows
                    }
                    selected_index = 0;
                    populate_file_list();
                }
                break;
            case 'f':
            case 'F':
                return NULL; // Exit browser
        }
        display_file_list();
    }
    return NULL;
}
