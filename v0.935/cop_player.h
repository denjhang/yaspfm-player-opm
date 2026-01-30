#ifndef COP_PLAYER_H
#define COP_PLAYER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// COP Player handle (opaque pointer)
typedef struct cop_player_t cop_player_t;

// SAA1099 register write callback
// Will be called for each register write during playback
typedef void (*saa_write_callback_t)(uint8_t reg, uint8_t data, void* user_data);

// Create COP player from file
// Returns NULL on error
cop_player_t* cop_player_create(const char* filename, saa_write_callback_t callback, void* user_data);

// Destroy player and free resources
void cop_player_destroy(cop_player_t* player);

// Get song information
bool cop_player_get_info(cop_player_t* player, char* title, size_t title_len,
                         char* author, size_t author_len, uint32_t* duration_ms);

// Reset player to beginning
void cop_player_reset(cop_player_t* player);

// Render next frame (50Hz for COP)
// Returns false when song ends
bool cop_player_render_frame(cop_player_t* player);

// Get total number of frames
uint32_t cop_player_get_total_frames(cop_player_t* player);

// Get current frame position
uint32_t cop_player_get_current_frame(cop_player_t* player);

// Seek to specific frame
void cop_player_seek(cop_player_t* player, uint32_t frame);

#ifdef __cplusplus
}
#endif

#endif /* COP_PLAYER_H */
