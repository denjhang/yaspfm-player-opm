#include "vgm.h"
#include "error.h"
#include "util.h"
#include "spfm.h"
#include "play.h"
#include "chiptype.h"
#include "ym2151.h"
#include "ym2612.h"
#include "ym2203.h"
#include "ym2413.h"
#include "sn76489.h"
#include "ay8910.h"
#include "y8950.h"
#include "ym3526.h"
#include "ym3812.h"
#include "ymf262.h"
#include "ym2608.h"
#include "adpcm.h"

#include <stdlib.h> // For malloc/free
#include <stdio.h>

extern volatile int g_flush_mode;

#define low_byte(x) ((x) & 0xff)
#define high_byte(x) (((x) >> 8) & 0xff)

bool vgm_parse_header(FILE *fp, struct vgm_header_t *header)
{
	size_t size;
	long data_offset;

	if ((size = fread(header, 1, VGM_HEADER_SIZE, fp)) != VGM_HEADER_SIZE) {
		logging(LOG_ERROR, "file size (%zu bytes) is smaller than VGM header size (%d bytes)\n", size, VGM_HEADER_SIZE);
		return false;
	}

	if (header->version < 0x150 || header->VGM_data_offset == 0)
		data_offset = VGM_DEFAULT_DATA_OFFSET;
	else
		data_offset = VGM_DATA_OFFSET_FROM + header->VGM_data_offset;

	if (fseek(fp, data_offset, SEEK_SET) < 0)
		return false;

	return true;
}

static bool opna_adpcm_write(FILE *input_fp, uint8_t type, uint32_t size)
{
    uint32_t rom_size, start_addr;
    size_t data_size = size - 8;
    uint8_t *data = (uint8_t*)malloc(data_size);
    if (!data) {
        logging(LOG_ERROR, "Failed to allocate memory for ADPCM data\n");
        return false;
    }

    if (fread(&rom_size, 1, 4, input_fp) != 4 || fread(&start_addr, 1, 4, input_fp) != 4) {
        logging(LOG_ERROR, "Failed to read ADPCM block header\n");
        free(data);
        return false;
    }

    if (fread(data, 1, data_size, input_fp) != data_size) {
        logging(LOG_ERROR, "Failed to read ADPCM data from file\n");
        free(data);
        return false;
    }

    if (type == 0x81) { // YM2608 DELTA-T
        logging(DEBUG, "Writing YM2608 ADPCM data. Address: 0x%X, Size: %zu\n", start_addr, data_size);
        spfm_write_ym2608_ram(get_slot_for_chip(CHIP_TYPE_YM2608), start_addr, data_size, data);
    } else if (type == 0x04) { // OKIM6258
        int pcm_len;
        int16_t* pcm = okim6258_adpcm_decode(data, data_size, &pcm_len);
        if (pcm) {
            int adpcm_len;
            uint8_t* adpcm = ym2608_adpcm_encode(pcm, pcm_len, &adpcm_len);
            if (adpcm) {
                logging(DEBUG, "Writing OKIM6258->YM2608 ADPCM data. Address: 0x%X, Size: %d\n", start_addr, adpcm_len);
                spfm_write_ym2608_ram(get_slot_for_chip(CHIP_TYPE_YM2608), start_addr, adpcm_len, adpcm);
                free(adpcm);
            }
            free(pcm);
        }
    } else {
        logging(WARN, "unsupported ADPCM type 0x%.2X\n", type);
    }

    free(data);
    spfm_flush(); // Ensure all data is sent before waiting
    yasp_usleep(50000); // Wait 50ms after RAM write, as in the reference implementation.
    return true;
}

int vgm_process_command(FILE *input_fp, struct vgm_header_t* header, int* vgm_wait1, int* vgm_wait2) {
    uint8_t op, buf[BUFSIZE];
    uint16_t u16_tmp;
    uint32_t u32_tmp;
    int wait_samples = 0;

    if (fread(&op, 1, 1, input_fp) != 1) {
        g_is_playing = false;
        return 0;
    }

    switch (op) {
        case 0x50: /* SN76489 */
            if (fread(buf, 1, 1, input_fp) != 1) { g_is_playing = false; return 0; }
            sn76489_write_reg(get_slot_for_chip(CHIP_TYPE_SN76489), buf[0]);
            break;
        case 0xA0: /* AY8910 */
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            ay8910_write_reg(get_slot_for_chip(CHIP_TYPE_AY8910), buf[0], buf[1]);
            break;
        case 0x5A: /* Y8950 */
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            y8950_write_reg(get_slot_for_chip(CHIP_TYPE_Y8950), buf[0], buf[1]);
            break;
        case 0x58: /* YM3526 */
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            ym3526_write_reg(get_slot_for_chip(CHIP_TYPE_YM3526), buf[0], buf[1]);
            break;
        case 0x59: /* YM3812 */
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            ym3812_write_reg(get_slot_for_chip(CHIP_TYPE_YM3812), buf[0], buf[1]);
            break;
        case 0x5E: /* YMF262 port 0 */
        case 0x5F: /* YMF262 port 1 */
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            ymf262_write_reg(get_slot_for_chip(CHIP_TYPE_YMF262), (op == 0x5E) ? 0 : 1, buf[0], buf[1]);
            break;
        case 0x51: /* YM2413 */
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            ym2413_write_reg(get_slot_for_chip(CHIP_TYPE_YM2413), buf[0], buf[1]);
            break;
        case 0x52: /* YM2612 port 0 */
        case 0x53: /* YM2612 port 1 */
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            ym2612_write_reg(get_slot_for_chip(CHIP_TYPE_YM2612), (op == 0x52) ? 0 : 1, buf[0], buf[1]);
            break;
        case 0x54: /* YM2151 */
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            ym2151_write_reg(get_slot_for_chip(CHIP_TYPE_YM2151), buf[0], buf[1]);
            break;
        case 0x55: /* YM2203 */
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            ym2203_write_reg(get_slot_for_chip(CHIP_TYPE_YM2203), buf[0], buf[1]);
            break;
        case 0x56: /* YM2608 normal */
        case 0x57: /* YM2608 extended */
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            ym2608_write_reg(get_slot_for_chip(CHIP_TYPE_YM2608), (op == 0x56) ? 0 : 1, buf[0], buf[1]);
            break;
        case 0x61:
            if (fread(&u16_tmp, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            wait_samples = u16_tmp;
            break;
        case 0x62:
            wait_samples = *vgm_wait1;
            break;
        case 0x63:
            wait_samples = *vgm_wait2;
            break;
        case 0x64:
            if (fread(buf, 1, 1, input_fp) != 1 || fread(&u16_tmp, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            if (buf[0] == 0x62) *vgm_wait1 = u16_tmp;
            else if (buf[0] == 0x63) *vgm_wait2 = u16_tmp;
            break;
        case 0x66: // End of sound data
            if (header->loop_offset > 0) {
                if (fseek(input_fp, (long)(header->loop_offset + 0x1C), SEEK_SET) != 0) {
                    g_is_playing = false;
                }
            } else {
                g_is_playing = false;
            }
            break;
        case 0x67:
            if (fread(buf, 1, 2, input_fp) != 2 || fread(&u32_tmp, 1, 4, input_fp) != 4) { g_is_playing = false; return 0; }
            if (buf[0] != 0x66) {
                logging(LOG_ERROR, "invalid sequence: 0x67 0x%.2X (expected 0x67 0x66)\n", buf[0]);
                g_is_playing = false; return 0;
            }
            opna_adpcm_write(input_fp, buf[1], u32_tmp);
            break;
        default:
            if (0x70 <= op && op <= 0x7F) {
                wait_samples = (op & 0x0F) + 1;
            } else {
                logging(DEBUG, "unknown VGM command:0x%.2X\n", op);
            }
            break;
    }
    if (g_flush_mode == 2) { // Command-level flush
        spfm_flush();
    }
    return wait_samples;
}

// Unified VGMPlay Mode
bool vgm_play_vgmplay_mode(FILE *fp, const char *filename) {
    struct vgm_header_t header;
    int vgm_wait1, vgm_wait2;
    extern volatile bool g_is_paused, g_next_track_flag, g_prev_track_flag, g_quit_flag, g_stop_current_song, g_random_mode;
    extern volatile double g_speed_multiplier;

    if (!vgm_parse_header(fp, &header)) return false;
    vgm_wait1 = VGM_DEFAULT_WAIT1;
    vgm_wait2 = VGM_DEFAULT_WAIT2;

    LARGE_INTEGER g_freq, g_last_counter;
    QueryPerformanceFrequency(&g_freq);
    
    HANDLE mm_timer_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    MMRESULT timer_id = timeSetEvent(1, 1, (LPTIMECALLBACK)mm_timer_event, 0, TIME_PERIODIC | TIME_CALLBACK_EVENT_SET);
    if (timer_id == 0) {
        logging(LOG_ERROR, "Failed to create multimedia timer for VGMPlay mode.\n");
        CloseHandle(mm_timer_event);
        return false; // Fail if the timer can't be created
    }

    QueryPerformanceCounter(&g_last_counter);
    double samples_per_tick = (double)VGM_SAMPLE_RATE / g_freq.QuadPart;
    double samples_to_process = 0;

    while (g_is_playing && !g_next_track_flag && !g_prev_track_flag && !g_stop_current_song && !g_quit_flag) {
        WaitForSingleObject(mm_timer_event, INFINITE);

        while (g_is_paused && !g_next_track_flag && !g_prev_track_flag && !g_stop_current_song && !g_quit_flag) {
            yasp_usleep(100000);
            QueryPerformanceCounter(&g_last_counter);
        }

        LARGE_INTEGER current_counter;
        QueryPerformanceCounter(&current_counter);
        samples_to_process += (current_counter.QuadPart - g_last_counter.QuadPart) * samples_per_tick * g_speed_multiplier;
        g_last_counter = current_counter;

        int samples_processed_this_loop = 0;
        while (samples_processed_this_loop < (int)samples_to_process) {
            int samples = vgm_process_command(fp, &header, &vgm_wait1, &vgm_wait2);
            if (samples > 0) {
                samples_processed_this_loop += samples;
            }
            if (!g_is_playing) break;
        }
        samples_to_process -= samples_processed_this_loop;
    }

    timeKillEvent(timer_id);
    CloseHandle(mm_timer_event);
    spfm_flush();
    g_is_playing = false;
    return true;
}

bool vgm_play_vgmplay_optimized(FILE *fp, const char *filename);

bool vgm_play(FILE *input_fp, const char *filename) {
    extern volatile int g_timer_mode;
    if (g_timer_mode == 3) { // Original VGMPlay mode
        return vgm_play_vgmplay_mode(input_fp, filename);
    }
    if (g_timer_mode == 7) { // New Optimized VGMPlay mode
        return vgm_play_vgmplay_optimized(input_fp, filename);
    }

    // --- Compensated Sleep approach for modes 0, 1, 2 ---
    struct vgm_header_t header;
    int vgm_wait1, vgm_wait2;
    extern volatile bool g_is_paused, g_next_track_flag, g_prev_track_flag, g_quit_flag, g_stop_current_song;
    extern volatile double g_speed_multiplier;

    if (!vgm_parse_header(input_fp, &header)) return false;
    vgm_wait1 = VGM_DEFAULT_WAIT1;
    vgm_wait2 = VGM_DEFAULT_WAIT2;

    LARGE_INTEGER g_freq, g_last_counter;
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_last_counter);
    double samples_per_tick = (double)VGM_SAMPLE_RATE / g_freq.QuadPart;
    double samples_to_process = 0;

    while (g_is_playing && !g_next_track_flag && !g_prev_track_flag && !g_stop_current_song && !g_quit_flag) {
        while (g_is_paused && !g_next_track_flag && !g_prev_track_flag && !g_stop_current_song && !g_quit_flag) {
            yasp_usleep(100000);
            QueryPerformanceCounter(&g_last_counter); // Reset timer after pause
        }

        // Calculate how many samples should have been processed based on real time
        LARGE_INTEGER current_counter;
        QueryPerformanceCounter(&current_counter);
        samples_to_process += (current_counter.QuadPart - g_last_counter.QuadPart) * samples_per_tick * g_speed_multiplier;
        g_last_counter = current_counter;

        // Process the accumulated samples
        int samples_to_run = (int)samples_to_process;
        if (samples_to_run > 0) {
            int samples_run_this_cycle = 0;
            while(samples_run_this_cycle < samples_to_run) {
                int s = vgm_process_command(input_fp, &header, &vgm_wait1, &vgm_wait2);
                if (s > 0) {
                    samples_run_this_cycle += s;
                }
                if (!g_is_playing) break;
            }
            samples_to_process -= samples_run_this_cycle;
        }
        
        // Yield CPU to avoid 100% usage
        yasp_usleep(1000); // Sleep 1ms
    }

    spfm_flush();
    g_is_playing = false;
    return true;
}

// New Optimized VGMPlay Mode
bool vgm_play_vgmplay_optimized(FILE *fp, const char *filename) {
    (void)filename; // filename is not used in this implementation
    struct vgm_header_t header;
    int vgm_wait1, vgm_wait2;
    extern volatile bool g_is_paused, g_next_track_flag, g_prev_track_flag, g_quit_flag, g_stop_current_song;
    extern volatile double g_speed_multiplier;

    if (!vgm_parse_header(fp, &header)) return false;
    vgm_wait1 = VGM_DEFAULT_WAIT1;
    vgm_wait2 = VGM_DEFAULT_WAIT2;

    LARGE_INTEGER g_freq, g_last_counter;
    QueryPerformanceFrequency(&g_freq);
    
    HANDLE mm_timer_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    MMRESULT timer_id = timeSetEvent(1, 1, (LPTIMECALLBACK)mm_timer_event, 0, TIME_PERIODIC | TIME_CALLBACK_EVENT_SET);
    if (timer_id == 0) {
        logging(LOG_ERROR, "Failed to create multimedia timer for Optimized VGMPlay mode.\n");
        CloseHandle(mm_timer_event);
        return false;
    }

    QueryPerformanceCounter(&g_last_counter);
    double samples_per_tick = (double)VGM_SAMPLE_RATE / g_freq.QuadPart;
    double samples_to_process = 0;
    const double max_samples_per_frame = VGM_SAMPLE_RATE / 60.0; // Limit processing to avoid huge catch-ups

    while (g_is_playing && !g_next_track_flag && !g_prev_track_flag && !g_stop_current_song && !g_quit_flag) {
        WaitForSingleObject(mm_timer_event, 2); // Wait for 1ms, with a 2ms timeout

        while (g_is_paused && !g_next_track_flag && !g_prev_track_flag && !g_stop_current_song && !g_quit_flag) {
            yasp_usleep(100000);
            QueryPerformanceCounter(&g_last_counter);
        }

        LARGE_INTEGER current_counter;
        QueryPerformanceCounter(&current_counter);
        samples_to_process += (current_counter.QuadPart - g_last_counter.QuadPart) * samples_per_tick * g_speed_multiplier;
        g_last_counter = current_counter;

        if (samples_to_process > max_samples_per_frame) {
            samples_to_process = max_samples_per_frame;
        }

        int samples_processed_this_loop = 0;
        while (samples_processed_this_loop < (int)samples_to_process) {
            int samples = vgm_process_command(fp, &header, &vgm_wait1, &vgm_wait2);
            if (samples > 0) {
                samples_processed_this_loop += samples;
            }
            if (!g_is_playing) break;
        }
        samples_to_process -= samples_processed_this_loop;
    }

    timeKillEvent(timer_id);
    CloseHandle(mm_timer_event);
    spfm_flush();
    g_is_playing = false;
    return true;
}
