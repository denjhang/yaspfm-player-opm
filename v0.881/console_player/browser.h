#ifndef BROWSER_H
#define BROWSER_H

#include <stdbool.h>

// Function to start the file browser.
// Takes the path of the currently playing song to start in the correct directory.
// Returns the path of the selected file to play, or NULL if no file was selected.
const char* file_browser(const char* current_song_path);

#endif // BROWSER_H
