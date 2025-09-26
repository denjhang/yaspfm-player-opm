/* See LICENSE for licence details. */
#ifndef PLAY_H
#define PLAY_H

#include <stdio.h>
#include <stdbool.h>
#include "ftd2xx.h" // We need this for FT_HANDLE

enum filetype_t {
	FILETYPE_S98 = 0,
	FILETYPE_VGM,
	FILETYPE_UNKNOWN,
};

extern volatile bool g_is_playing;

bool play_file(const char *path);
void update_ui(int current_samples, int total_samples, const char* song_name, bool paused, bool random);

#endif /* PLAY_H */
