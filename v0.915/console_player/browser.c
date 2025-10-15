#include "error.h"
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
#include <sys/stat.h>
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
static bool is_drive_list = false;

#define PAGE_SIZE 10
static int current_page = 0;

// Function to normalize path separators to forward slashes
void normalize_path(char* path) {
    for (char* p = path; *p; ++p) {
        if (*p == '\\') {
            *p = '/';
        }
    }
    // Remove trailing slash if not root
    size_t len = strlen(path);
    if (len > 1 && path[len - 1] == '/') {
         // but keep it for "C:/"
        if (!(len == 3 && path[1] == ':')) {
            path[len - 1] = '\0';
        }
    }
}

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
    is_drive_list = false;
    normalize_path(current_path);

#ifdef _WIN32
    if (strcmp(current_path, ".") == 0 || strcmp(current_path, "") == 0) {
        DWORD drives = GetLogicalDrives();
        if (drives != 0) {
            for (int i = 0; i < 26; i++) {
                if ((drives >> i) & 1) {
                    if (file_count < MAX_FILES) {
                        snprintf(file_list[file_count].name, 256, "%c:", 'A' + i);
                        file_list[file_count].is_dir = true;
                        file_count++;
                    }
                }
            }
            is_drive_list = true;
            strcpy(current_path, "."); // Set a clear state
            return;
        }
        // If no drives found, proceed to scan current dir
    }
#endif

    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(current_path))) {
        // If opendir fails, it might be because we are at the root of a drive.
        // In that case, we can go back to the drive list.
        strcpy(current_path, ".");
        populate_file_list();
        return;
    }

    while ((entry = readdir(dir)) != NULL && file_count < MAX_FILES) {
        if (strcmp(entry->d_name, ".") == 0) continue;

        FileEntry fe;
        strncpy(fe.name, entry->d_name, sizeof(fe.name) - 1);
        fe.name[sizeof(fe.name) - 1] = '\0';

        // Optimization: Use d_type if available to avoid stat calls
        #if defined(_DIRENT_HAVE_D_TYPE) && !defined(_WIN32)
        if (entry->d_type != DT_UNKNOWN && entry->d_type != DT_LNK) {
            fe.is_dir = (entry->d_type == DT_DIR);
        } else
        #endif
        {
            // Fallback to stat/GetFileAttributes if d_type is not available or is a symlink
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
        }
        
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

const char* file_browser(const char* initial_path) {
    // Set initial path
    if (initial_path) {
        strncpy(current_path, initial_path, sizeof(current_path) - 1);
    } else {
        strcpy(current_path, ".");
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
                    if (file_count == 0) break; // Nothing to select
                    if (file_list[selected_index].is_dir) {
                        if (is_drive_list) {
                            snprintf(current_path, sizeof(current_path), "%s/", file_list[selected_index].name);
                        } else if (strcmp(file_list[selected_index].name, "..") == 0) {
                            // Go up
                            normalize_path(current_path);
                            char* last_slash = strrchr(current_path, '/');
                            if (last_slash) {
                                // If path is "C:/", last_slash points to the slash.
                                // After this, path becomes "C:", which is not a valid directory.
                                if (last_slash == current_path || (strlen(current_path) == 3 && current_path[1] == ':')) {
                                     // At root (e.g., "/" or "C:/"), go to drive list
                                    strcpy(current_path, ".");
                                } else {
                                    *last_slash = '\0';
                                }
                            } else {
                                // No slash, we are in a root-level directory like "C:" or in "."
                                strcpy(current_path, ".");
                            }
                        } else {
                            // Go down
                            if (strcmp(current_path, ".") != 0) {
                                strncat(current_path, "/", sizeof(current_path) - strlen(current_path) - 1);
                            } else {
                                // If current path is ".", just start with the new name
                                current_path[0] = '\0';
                            }
                            strncat(current_path, file_list[selected_index].name, sizeof(current_path) - strlen(current_path) - 1);
                        }
                        normalize_path(current_path);
                        selected_index = 0;
                        current_page = 0;
                        populate_file_list();
                    } else {
                        // Play file
                        snprintf(selected_file_path, sizeof(selected_file_path), "%s/%s", current_path, file_list[selected_index].name);
                        normalize_path(selected_file_path);
                        logging(LOG_LEVEL_DEBUG, "File selected in browser: %s", selected_file_path);
                        return selected_file_path;
                    }
                    break;
                case 8: // Backspace
                     if (is_drive_list) break; // Can't go up from drive list
                    // Simulate selecting ".."
                    {
                        normalize_path(current_path);
                        char* last_slash = strrchr(current_path, '/');
                        if (last_slash) {
                            if (strlen(current_path) == 3 && current_path[1] == ':') {
                                strcpy(current_path, ".");
                            } else {
                                *last_slash = '\0';
                            }
                        } else {
                            strcpy(current_path, ".");
                        }
                        selected_index = 0;
                        current_page = 0;
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
