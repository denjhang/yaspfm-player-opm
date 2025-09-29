/* See LICENSE for licence details. */
#ifndef PLAY_H
#define PLAY_H

#define MAX_FILENAME_LEN 256

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "ftd2xx.h" // We need this for FT_HANDLE

enum filetype_t {
	FILETYPE_S98 = 0,
	FILETYPE_VGM,
	FILETYPE_UNKNOWN,
};

extern volatile bool g_is_playing;
extern volatile bool g_stop_current_song;

bool play_file(const char *path);
void update_ui(uint32_t total_samples, const char* song_name, bool paused, int play_mode);
bool vgm_play_vgmplay_mode(FILE *fp, const char *filename);

// UI utility functions
void init_ui();
void clear_line(int y);
void print_at(int x, int y, const char* text);

#endif /* PLAY_H */
