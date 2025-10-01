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
#include "opn_to_opm.h"
#include "ay_to_opm.h"
#include "sn_to_ay.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#ifdef _WIN32
#include <direct.h> // For _mkdir
#else
#include <sys/stat.h> // For mkdir
#endif

extern volatile int g_flush_mode;
extern volatile int g_vgm_loop_count;
extern int g_current_song_total_samples;
extern volatile cache_mode_t g_cache_mode;

// --- Conversion State ---
bool g_opn_to_opm_conversion_enabled = false;
bool g_ay_to_opm_conversion_enabled = false;
bool g_sn_to_ay_conversion_enabled = false;
volatile bool g_is_playing_from_cache = false;
static bool g_is_conversion_active = false;
static uint32_t g_original_loop_offset = 0;
static uint32_t g_converted_loop_offset = 0;
chip_type_t g_vgm_chip_type = CHIP_TYPE_NONE;
chip_type_t g_original_vgm_chip_type = CHIP_TYPE_NONE; // To store the chip type of the original file
vgm_header_t g_vgm_header;

// --- OPM Writer Callbacks ---
static FILE* g_cache_fp = NULL;

static void spfm_opm_writer(uint8_t addr, uint8_t data) {
    spfm_write_reg(get_slot_for_chip(CHIP_TYPE_YM2151), 0, addr, data);
}

static void vgm_cache_opm_writer(uint8_t addr, uint8_t data) {
    if (g_cache_fp) {
        fputc(0x54, g_cache_fp); // YM2151 write
        fputc(addr, g_cache_fp);
        fputc(data, g_cache_fp);
    }
}

// --- Chained Converters for Caching ---
static void sn_to_ay_bridge(uint8_t addr, uint8_t data) {
    ay_to_opm_write_reg(addr, data);
}

static bool vgm_convert_and_cache_from_mem(const uint8_t* vgm_data, size_t vgm_data_size, const vgm_header_t* original_header);
static chip_type_t get_primary_chip_from_header(const vgm_header_t* header);
static uint32_t get_clock_from_header(const vgm_header_t* header, chip_type_t chip_type);


// Helper to read little-endian 16-bit integer
static uint16_t read_le16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

// Helper to read a relative offset
static uint32_t read_rel_ofs(const uint8_t* data, uint32_t base_ofs) {
    uint32_t ofs = read_le32(data + base_ofs);
    return ofs ? (base_ofs + ofs) : 0;
}

// Helper to write little-endian 32-bit integer
static void write_le32(uint8_t* data, uint32_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}

// Helper function to read a UTF-16LE string from the GD3 tag
static void read_gd3_string(FILE* f, long end_pos, wchar_t* dest, size_t max_len) {
    long initial_pos = ftell(f);
    size_t i = 0;
    while (ftell(f) < end_pos && i < max_len - 1) {
        uint16_t ch;
        if (fread(&ch, 2, 1, f) != 1) break;
        if (ch == 0) break;
        dest[i++] = (wchar_t)ch;
    }
    dest[i] = L'\0';
    if (i == 0) {
        fseek(f, initial_pos + 2, SEEK_SET);
    }
}

bool vgm_parse_header(FILE* fp, vgm_header_t* header) {
    uint8_t hdr_buf[0x100];
    memset(header, 0, sizeof(vgm_header_t));

    if (fseek(fp, 0, SEEK_SET) != 0 || fread(hdr_buf, 1, 0x40, fp) != 0x40) {
        logging(LOG_ERROR, "Could not read first 0x40 bytes of header.\n");
        return false;
    }

    if (memcmp(hdr_buf, "Vgm ", 4) != 0) {
        logging(LOG_ERROR, "Invalid VGM signature.\n");
        return false;
    }
    memcpy(header->ident, hdr_buf, 4);

    header->version = read_le32(hdr_buf + 0x08);
    
    size_t header_size_to_read = 0;
    if (header->version >= 0x171) header_size_to_read = 0xE4;
    else if (header->version >= 0x170) header_size_to_read = 0xC0;
    else if (header->version >= 0x161) header_size_to_read = 0xB8;
    else if (header->version >= 0x151) header_size_to_read = 0x80;
    else header_size_to_read = 0x40;

    if (header_size_to_read > 0x40) {
        if (fread(hdr_buf + 0x40, 1, header_size_to_read - 0x40, fp) != (header_size_to_read - 0x40)) {
            logging(WARN, "Could not read full extended header. Some data may be missing.\n");
        }
    }

    header->eof_offset = read_le32(hdr_buf + 0x04);
    header->total_samples = read_le32(hdr_buf + 0x18);
    header->loop_offset = read_rel_ofs(hdr_buf, 0x1C);
    header->loop_samples = read_le32(hdr_buf + 0x20);
    
    if (header->version >= 0x150) {
        header->vgm_data_offset = read_rel_ofs(hdr_buf, 0x34);
    } else {
        header->vgm_data_offset = 0x40;
    }
    if (header->vgm_data_offset == 0) header->vgm_data_offset = 0x40;

    header->sn76489_clock = read_le32(hdr_buf + 0x0C);
    header->ym2413_clock = read_le32(hdr_buf + 0x10);

    if (header->version >= 0x101) header->rate = read_le32(hdr_buf + 0x24);
    if (header->version >= 0x110) {
        header->sn76489_feedback = read_le16(hdr_buf + 0x28);
        header->sn76489_shift_width = hdr_buf[0x2A];
        header->ym2612_clock = read_le32(hdr_buf + 0x2C);
        header->ym2151_clock = read_le32(hdr_buf + 0x30);
    }
    if (header->version >= 0x151) {
        header->ym2203_clock = read_le32(hdr_buf + 0x44);
        header->ym2608_clock = read_le32(hdr_buf + 0x48);
        header->ay8910_clock = read_le32(hdr_buf + 0x74);
    }
    
    uint32_t gd3_offset = read_rel_ofs(hdr_buf, 0x14);
    if (gd3_offset > 0) {
        if (fseek(fp, gd3_offset, SEEK_SET) == 0) {
            char gd3_sig[4];
            if (fread(gd3_sig, 1, 4, fp) == 4 && memcmp(gd3_sig, "Gd3 ", 4) == 0) {
                uint32_t gd3_version, gd3_length;
                fread(&gd3_version, 4, 1, fp);
                fread(&gd3_length, 4, 1, fp);
                long end_pos = ftell(fp) + gd3_length;
                read_gd3_string(fp, end_pos, header->track_name_en, 256);
                read_gd3_string(fp, end_pos, header->track_name_jp, 256);
                read_gd3_string(fp, end_pos, header->game_name_en, 256);
                read_gd3_string(fp, end_pos, header->game_name_jp, 256);
                read_gd3_string(fp, end_pos, header->system_name_en, 256);
                read_gd3_string(fp, end_pos, header->system_name_jp, 256);
                read_gd3_string(fp, end_pos, header->author_en, 256);
                read_gd3_string(fp, end_pos, header->author_jp, 256);
                read_gd3_string(fp, end_pos, header->release_date, 256);
                read_gd3_string(fp, end_pos, header->vgm_creator, 256);
                read_gd3_string(fp, end_pos, header->notes, 256);
            }
        }
    }

    fseek(fp, header->vgm_data_offset, SEEK_SET);
    return true;
}

int vgm_process_command(FILE *input_fp, int* vgm_wait1, int* vgm_wait2, int* loop_counter) {
    uint8_t op, buf[2];
    uint16_t u16_tmp;
    int wait_samples = 0;

    if (fread(&op, 1, 1, input_fp) != 1) {
        g_is_playing = false;
        return 0;
    }

    switch (op) {
        case 0x50:
            if (fread(buf, 1, 1, input_fp) != 1) { g_is_playing = false; return 0; }
            if (g_sn_to_ay_conversion_enabled) sn_to_ay_write_reg(buf[0]);
            else sn76489_write_reg(get_slot_for_chip(CHIP_TYPE_SN76489), buf[0]);
            break;
        case 0xA0:
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            if (g_ay_to_opm_conversion_enabled) ay_to_opm_write_reg(buf[0], buf[1]);
            else ay8910_write_reg(get_slot_for_chip(CHIP_TYPE_AY8910), buf[0], buf[1]);
            break;
        case 0x52: case 0x53:
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            if (g_opn_to_opm_conversion_enabled && g_vgm_chip_type == CHIP_TYPE_YM2612) opn_to_opm_write_reg(buf[0], buf[1], (op == 0x52) ? 0 : 1);
            else ym2612_write_reg(get_slot_for_chip(CHIP_TYPE_YM2612), (op == 0x52) ? 0 : 1, buf[0], buf[1]);
            break;
        case 0x54:
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            ym2151_write_reg(get_slot_for_chip(CHIP_TYPE_YM2151), buf[0], buf[1]);
            break;
        case 0x55:
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            if (g_opn_to_opm_conversion_enabled && g_vgm_chip_type == CHIP_TYPE_YM2203) opn_to_opm_write_reg(buf[0], buf[1], 0);
            else ym2203_write_reg(get_slot_for_chip(CHIP_TYPE_YM2203), buf[0], buf[1]);
            break;
        case 0x56: case 0x57:
            if (fread(buf, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            if (g_opn_to_opm_conversion_enabled && g_vgm_chip_type == CHIP_TYPE_YM2608) opn_to_opm_write_reg(buf[0], buf[1], (op == 0x56) ? 0 : 1);
            else ym2608_write_reg(get_slot_for_chip(CHIP_TYPE_YM2608), (op == 0x56) ? 0 : 1, buf[0], buf[1]);
            break;
        case 0x61:
            if (fread(&u16_tmp, 1, 2, input_fp) != 2) { g_is_playing = false; return 0; }
            wait_samples = u16_tmp;
            break;
        case 0x62: wait_samples = *vgm_wait1; break;
        case 0x63: wait_samples = *vgm_wait2; break;
        case 0x66:
            if (g_vgm_header.loop_offset > 0 && (*loop_counter < g_vgm_loop_count || g_vgm_loop_count == 0)) {
                if (fseek(input_fp, (long)(g_vgm_header.loop_offset), SEEK_SET) != 0) g_is_playing = false;
                (*loop_counter)++;
            } else g_is_playing = false;
            break;
        default:
            if (0x70 <= op && op <= 0x7F) wait_samples = (op & 0x0F) + 1;
            break;
    }
    if (g_flush_mode == 2) spfm_flush();
    return wait_samples;
}

FILE* vgm_play(FILE *input_fp, const char *filename, const char *cache_filename, bool force_reconvert) {
    (void)filename;
    extern volatile bool g_next_track_flag, g_prev_track_flag, g_quit_flag, g_stop_current_song;
    
    FILE* current_fp = input_fp;
    bool is_realtime_conversion = false;

    // Always parse the header of the original file first to get original chip type and GD3.
    if (!vgm_parse_header(current_fp, &g_vgm_header)) {
        g_current_song_total_samples = 0;
        return current_fp; // Return original fp to be closed by caller
    }
    g_current_song_total_samples = g_vgm_header.total_samples;
    g_original_vgm_chip_type = get_primary_chip_from_header(&g_vgm_header);
    g_vgm_chip_type = g_original_vgm_chip_type;
    
    bool needs_opn_conversion = (g_vgm_chip_type == CHIP_TYPE_YM2612 || g_vgm_chip_type == CHIP_TYPE_YM2203 || g_vgm_chip_type == CHIP_TYPE_YM2608);
    bool opm_available = (get_slot_for_chip(CHIP_TYPE_YM2151) != 0xFF);

    g_is_playing_from_cache = false;
    g_opn_to_opm_conversion_enabled = false;
    g_ay_to_opm_conversion_enabled = false;
    g_sn_to_ay_conversion_enabled = false;
    g_is_conversion_active = false;

    bool needs_ay_conversion = (g_vgm_chip_type == CHIP_TYPE_AY8910);
    bool needs_sn_conversion = (g_vgm_chip_type == CHIP_TYPE_SN76489);

    if ((needs_opn_conversion || needs_ay_conversion || needs_sn_conversion) && opm_available) {
        g_is_conversion_active = true; // Mark that some conversion is happening
        // Set global flags for UI display
        if (needs_opn_conversion) g_opn_to_opm_conversion_enabled = true;
        if (needs_ay_conversion) g_ay_to_opm_conversion_enabled = true;
        if (needs_sn_conversion) g_sn_to_ay_conversion_enabled = true;

        FILE* cache_fp_read = NULL;
        if (g_cache_mode == CACHE_MODE_NORMAL && !force_reconvert) {
            cache_fp_read = fopen(cache_filename, "rb");
        }

        if (cache_fp_read) {
            // --- CACHE EXISTS ---
            logging(INFO, "Found cache file: %s. Playing from cache.", cache_filename);
            fclose(current_fp); // Close original file
            current_fp = cache_fp_read;
            g_is_playing_from_cache = true;
            if (!vgm_parse_header(current_fp, &g_vgm_header)) { 
                // Don't close current_fp here, let the caller do it.
                g_current_song_total_samples = 0;
                return current_fp; 
            }
            g_current_song_total_samples = g_vgm_header.total_samples;
            g_vgm_chip_type = get_primary_chip_from_header(&g_vgm_header);
        } else {
            // --- CACHE DOES NOT EXIST, CREATE IT ---
            logging(INFO, "Cache not found. Converting %s to OPM...", chip_type_to_string(g_vgm_chip_type));

#ifdef _WIN32
            _mkdir("console_player/cache");
#else
            mkdir("console_player/cache", 0755);
#endif
            
            // 1. Read entire original file into memory
            fseek(input_fp, 0, SEEK_END);
            long original_file_size = ftell(input_fp);
            fseek(input_fp, 0, SEEK_SET);
            uint8_t* original_file_data = malloc(original_file_size);
            if (!original_file_data || fread(original_file_data, 1, original_file_size, input_fp) != original_file_size) {
                logging(LOG_ERROR, "Failed to read original VGM file into memory.");
                if (original_file_data) free(original_file_data);
                // Don't close input_fp here, it's already closed or invalid.
                // Return the original pointer for the caller to attempt cleanup.
                return input_fp;
            }
            // We are done with the original file, close it. The caller no longer needs to.
            fclose(input_fp); 

            // 2. Open cache file for writing
            g_cache_fp = fopen(cache_filename, "wb");
            if (!g_cache_fp) {
                logging(LOG_ERROR, "Failed to open cache file for writing: %s", cache_filename);
                free(original_file_data);
                return NULL; // Return NULL as we couldn't proceed.
            }

            // 3. Write placeholder header and convert data
            uint8_t header_buf[0x100] = {0};
            fwrite(header_buf, 1, 0x100, g_cache_fp);
            long data_start_offset = ftell(g_cache_fp);

            const uint8_t* vgm_data_ptr = original_file_data + g_vgm_header.vgm_data_offset;
            size_t vgm_data_size = (g_vgm_header.eof_offset + 4) - g_vgm_header.vgm_data_offset;
            vgm_convert_and_cache_from_mem(vgm_data_ptr, vgm_data_size, &g_vgm_header);

            // 4. Write GD3 block from memory to cache
            long gd3_start_in_cache = 0;
            uint32_t gd3_offset_in_header = read_le32(original_file_data + 0x14);
            if (gd3_offset_in_header > 0) {
                uint32_t gd3_abs_offset = 0x14 + gd3_offset_in_header;
                uint32_t gd3_length = read_le32(original_file_data + gd3_abs_offset + 8);
                uint32_t total_gd3_size = 12 + gd3_length;
                
                gd3_start_in_cache = ftell(g_cache_fp);
                fwrite(original_file_data + gd3_abs_offset, 1, total_gd3_size, g_cache_fp);
            }

            // 5. Finalize and write the real header
            long final_file_size = ftell(g_cache_fp);
            memset(header_buf, 0, 0x100);
            memcpy(header_buf, "Vgm ", 4);
            write_le32(header_buf + 0x04, final_file_size - 4);
            write_le32(header_buf + 0x08, g_vgm_header.version);
            if (gd3_start_in_cache > 0) write_le32(header_buf + 0x14, gd3_start_in_cache - 0x14);
            write_le32(header_buf + 0x18, g_vgm_header.total_samples); // This might need recalculation
            // --- Copy loop data to cached file ---
            if (g_converted_loop_offset > 0 && g_vgm_loop_count != 1) {
                 write_le32(header_buf + 0x1C, g_converted_loop_offset - 0x1C);
                 write_le32(header_buf + 0x20, g_vgm_header.loop_samples);
            } else {
                 write_le32(header_buf + 0x1C, 0);
                 write_le32(header_buf + 0x20, 0);
            }
            write_le32(header_buf + 0x24, g_vgm_header.rate);
            write_le32(header_buf + 0x30, get_chip_default_clock(CHIP_TYPE_YM2151));
            if (g_vgm_header.version >= 0x150) write_le32(header_buf + 0x34, data_start_offset - 0x34);

            fseek(g_cache_fp, 0, SEEK_SET);
            fwrite(header_buf, 1, 0x100, g_cache_fp);
            fclose(g_cache_fp);
            g_cache_fp = NULL;
            free(original_file_data);

            // 6. Re-open the newly created cache file for playing
            current_fp = fopen(cache_filename, "rb");
            if (!current_fp) { 
                logging(LOG_ERROR, "Failed to open newly created cache file!"); 
                return NULL; // Return NULL, caller has nothing to close.
            }
            g_is_playing_from_cache = true;
            if (!vgm_parse_header(current_fp, &g_vgm_header)) { 
                // Let caller close the fp
                return current_fp; 
            }
            g_current_song_total_samples = g_vgm_header.total_samples;
            g_vgm_chip_type = get_primary_chip_from_header(&g_vgm_header);
        }
    }

    if (!is_realtime_conversion) {
        fseek(current_fp, g_vgm_header.vgm_data_offset, SEEK_SET);
    }

    g_is_playing = true;
    
    #ifdef _WIN32
    HANDLE h_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)vgm_player_thread, current_fp, 0, NULL);
    if (h_thread) {
        while (g_is_playing && !g_next_track_flag && !g_prev_track_flag && !g_stop_current_song && !g_quit_flag) {
            if (WaitForSingleObject(h_thread, 16) == WAIT_OBJECT_0) break;
        }
        CloseHandle(h_thread);
    }
    #else
    vgm_player_thread(current_fp);
    #endif

    return current_fp;
}

DWORD WINAPI vgm_player_thread(LPVOID lpParam) {
    FILE* input_fp = (FILE*)lpParam;
    extern volatile int g_timer_mode;

    // VGMPlay Mode (most accurate, uses multimedia timer)
    if (g_timer_mode == 3) {
        int vgm_wait1 = VGM_DEFAULT_WAIT1, vgm_wait2 = VGM_DEFAULT_WAIT2;
        int loop_counter = 1;
        extern volatile bool g_is_paused, g_next_track_flag, g_prev_track_flag, g_quit_flag, g_stop_current_song;
        extern volatile double g_speed_multiplier;

        LARGE_INTEGER g_freq, g_last_counter;
        QueryPerformanceFrequency(&g_freq);
        
        HANDLE mm_timer_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        MMRESULT timer_id = timeSetEvent(1, 1, (LPTIMECALLBACK)mm_timer_event, 0, TIME_PERIODIC | TIME_CALLBACK_EVENT_SET);
        if (timer_id == 0) {
            logging(LOG_ERROR, "Failed to create multimedia timer for VGMPlay mode.\n");
            if(mm_timer_event) CloseHandle(mm_timer_event);
            g_is_playing = false;
            return 1;
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
                int samples = vgm_process_command(input_fp, &vgm_wait1, &vgm_wait2, &loop_counter);
                if (samples > 0) {
                    samples_processed_this_loop += samples;
                }
                if (!g_is_playing) break;
            }
            samples_to_process -= samples_processed_this_loop;
        }

        timeKillEvent(timer_id);
        CloseHandle(mm_timer_event);
    } 
    // Compensated Sleep Mode (less accurate but good fallback for other modes)
    else {
        int vgm_wait1 = VGM_DEFAULT_WAIT1, vgm_wait2 = VGM_DEFAULT_WAIT2;
        int loop_counter = 1;
        extern volatile bool g_is_paused, g_next_track_flag, g_prev_track_flag, g_quit_flag, g_stop_current_song;
        extern volatile double g_speed_multiplier;

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

            LARGE_INTEGER current_counter;
            QueryPerformanceCounter(&current_counter);
            samples_to_process += (current_counter.QuadPart - g_last_counter.QuadPart) * samples_per_tick * g_speed_multiplier;
            g_last_counter = current_counter;

            int samples_to_run = (int)samples_to_process;
            if (samples_to_run > 0) {
                int samples_run_this_cycle = 0;
                while(samples_run_this_cycle < samples_to_run) {
                    int s = vgm_process_command(input_fp, &vgm_wait1, &vgm_wait2, &loop_counter);
                    if (s > 0) {
                        samples_run_this_cycle += s;
                    }
                    if (!g_is_playing) break;
                }
                samples_to_process -= samples_run_this_cycle;
            }
            
            yasp_usleep(1000); // Sleep 1ms to yield CPU
        }
    }

    spfm_flush();
    g_is_playing = false;
    return 0;
}

static bool vgm_convert_and_cache_from_mem(const uint8_t* vgm_data, size_t vgm_data_size, const vgm_header_t* original_header) {
    chip_type_t original_chip_type = get_primary_chip_from_header(original_header);
    uint32_t original_clock = get_clock_from_header(original_header, original_chip_type);

    g_original_loop_offset = original_header->loop_offset;
    g_converted_loop_offset = 0;

    // Setup the conversion chain
    if (g_opn_to_opm_conversion_enabled) {
        opn_to_opm_init(original_chip_type, original_clock, vgm_cache_opm_writer);
    } else if (g_ay_to_opm_conversion_enabled) {
        ay_to_opm_init(original_chip_type, original_clock, vgm_cache_opm_writer);
    } else if (g_sn_to_ay_conversion_enabled) {
        ay_to_opm_init(CHIP_TYPE_AY8910, get_chip_default_clock(CHIP_TYPE_AY8910), vgm_cache_opm_writer);
        sn_to_ay_init(original_chip_type, original_clock, sn_to_ay_bridge);
    }

    size_t pos = 0;
    while (pos < vgm_data_size) {
        // Check for loop point
        if (g_original_loop_offset > 0 && (original_header->vgm_data_offset + pos) >= g_original_loop_offset && g_converted_loop_offset == 0) {
            g_converted_loop_offset = ftell(g_cache_fp);
        }

        uint8_t op = vgm_data[pos++];
        uint8_t d1, d2;

        switch (op) {
            // --- Convertible Chips ---
            case 0x52: case 0x53: { // YM2612
                if (pos + 2 > vgm_data_size) goto end_convert_loop;
                d1 = vgm_data[pos++]; d2 = vgm_data[pos++];
                if (g_opn_to_opm_conversion_enabled && original_chip_type == CHIP_TYPE_YM2612) {
                    opn_to_opm_write_reg(d1, d2, (op == 0x52) ? 0 : 1);
                } else {
                    fputc(op, g_cache_fp); fputc(d1, g_cache_fp); fputc(d2, g_cache_fp);
                }
                break;
            }
            case 0x55: { // YM2203
                if (pos + 2 > vgm_data_size) goto end_convert_loop;
                d1 = vgm_data[pos++]; d2 = vgm_data[pos++];
                if (g_opn_to_opm_conversion_enabled && original_chip_type == CHIP_TYPE_YM2203) {
                    opn_to_opm_write_reg(d1, d2, 0);
                } else {
                    fputc(op, g_cache_fp); fputc(d1, g_cache_fp); fputc(d2, g_cache_fp);
                }
                break;
            }
            case 0x56: case 0x57: { // YM2608
                if (pos + 2 > vgm_data_size) goto end_convert_loop;
                d1 = vgm_data[pos++]; d2 = vgm_data[pos++];
                if (g_opn_to_opm_conversion_enabled && original_chip_type == CHIP_TYPE_YM2608) {
                    opn_to_opm_write_reg(d1, d2, (op == 0x56) ? 0 : 1);
                } else {
                    fputc(op, g_cache_fp); fputc(d1, g_cache_fp); fputc(d2, g_cache_fp);
                }
                break;
            }
            case 0xA0: { // AY8910
                if (pos + 2 > vgm_data_size) goto end_convert_loop;
                d1 = vgm_data[pos++]; d2 = vgm_data[pos++];
                if (g_ay_to_opm_conversion_enabled && original_chip_type == CHIP_TYPE_AY8910) {
                    ay_to_opm_write_reg(d1, d2);
                } else {
                    fputc(op, g_cache_fp); fputc(d1, g_cache_fp); fputc(d2, g_cache_fp);
                }
                break;
            }
            case 0x50: { // SN76489
                if (pos + 1 > vgm_data_size) goto end_convert_loop;
                d1 = vgm_data[pos++];
                if (g_sn_to_ay_conversion_enabled && original_chip_type == CHIP_TYPE_SN76489) {
                    sn_to_ay_write_reg(d1);
                } else {
                    fputc(op, g_cache_fp); fputc(d1, g_cache_fp);
                }
                break;
            }

            // --- Non-Convertible Chips (Passthrough) ---
            case 0x54: // YM2151
                if (pos + 2 > vgm_data_size) goto end_convert_loop;
                d1 = vgm_data[pos++]; d2 = vgm_data[pos++];
                // If we are doing a conversion TO OPM, we must drop any
                // stray YM2151 commands from the source file to avoid conflicts.
                if (!g_is_conversion_active) {
                    fputc(op, g_cache_fp); fputc(d1, g_cache_fp); fputc(d2, g_cache_fp);
                }
                break;
            case 0x51: case 0x58: case 0x59: case 0x5A: case 0x5B:
            case 0x5C: case 0x5D: case 0x5E: case 0x5F: case 0xB0: case 0xB1:
            case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
            case 0xB8: case 0xC3: case 0xC4: case 0xC5: case 0xC6: case 0xC7:
            case 0xC8: case 0xD0: case 0xD1: case 0xD2: case 0xD3: case 0xD4:
            case 0xD5: case 0xD6: case 0xE1:
                if (pos + 2 > vgm_data_size) goto end_convert_loop;
                d1 = vgm_data[pos++]; d2 = vgm_data[pos++];
                fputc(op, g_cache_fp); fputc(d1, g_cache_fp); fputc(d2, g_cache_fp);
                break;

            // --- Other 1-byte operand commands (Passthrough) ---
            case 0x4F: case 0x90: case 0x91: case 0x95:
                if (pos + 1 > vgm_data_size) goto end_convert_loop;
                d1 = vgm_data[pos++];
                fputc(op, g_cache_fp); fputc(d1, g_cache_fp);
                break;

            // --- Common Commands (Passthrough) ---
            case 0x61: {
                if (pos + 2 > vgm_data_size) goto end_convert_loop;
                uint16_t wait = read_le16(&vgm_data[pos]);
                pos += 2;
                if (g_ay_to_opm_conversion_enabled) {
                    for (int i = 0; i < wait; i++) {
                        ay_to_opm_update_envelope();
                    }
                }
                fputc(op, g_cache_fp);
                fwrite(&wait, 2, 1, g_cache_fp);
                break;
            }
            case 0x62: case 0x63: case 0x66:
                fputc(op, g_cache_fp);
                break;

            // --- Commands to properly parse and skip ---
            case 0x67: { // Data block
                if (pos + 6 > vgm_data_size) goto end_convert_loop;
                if (vgm_data[pos] == 0x66) {
                    uint32_t block_size = read_le32(&vgm_data[pos + 2]);
                    if (pos + 6 + block_size > vgm_data_size) goto end_convert_loop;
                    pos += 6 + block_size;
                } else {
                    pos += 6; // Should not happen, but as a fallback
                }
                break;
            }
            case 0xE0: // PCM data seek
                if (pos + 4 > vgm_data_size) goto end_convert_loop;
                pos += 4;
                break;
            
            // --- Special Handling ---
            default:
                if (op >= 0x70 && op <= 0x7F) { // Wait n+1 samples
                    if (g_ay_to_opm_conversion_enabled) {
                        for (int i = 0; i < (op & 0x0F) + 1; i++) {
                            ay_to_opm_update_envelope();
                        }
                    }
                    fputc(op, g_cache_fp);
                } else if (op >= 0x80 && op <= 0x8F) { // PCM wait
                    if (pos + 1 > vgm_data_size) goto end_convert_loop;
                    pos++; // Skip the data byte for the PCM write.
                    uint8_t n_wait = op & 0x0F;
                    if (n_wait > 0) {
                        fputc(0x70 | (n_wait - 1), g_cache_fp);
                    }
                }
                // Any other unknown command is simply dropped.
                break;
        }
    }
end_convert_loop:;
    fputc(0x66, g_cache_fp); // Write final END command
    return true;
}

static chip_type_t get_primary_chip_from_header(const vgm_header_t* header) {
    if (header->ym2608_clock > 0) return CHIP_TYPE_YM2608;
    if (header->ym2612_clock > 0) return CHIP_TYPE_YM2612;
    if (header->ym2203_clock > 0) return CHIP_TYPE_YM2203;
    if (header->ym2151_clock > 0) return CHIP_TYPE_YM2151;
    if (header->ay8910_clock > 0) return CHIP_TYPE_AY8910;
    if (header->sn76489_clock > 0) return CHIP_TYPE_SN76489;
    if (header->ym2413_clock > 0) return CHIP_TYPE_YM2413;
    return CHIP_TYPE_NONE;
}

static uint32_t get_clock_from_header(const vgm_header_t* header, chip_type_t chip_type) {
    switch (chip_type) {
        case CHIP_TYPE_YM2608: return header->ym2608_clock;
        case CHIP_TYPE_YM2612: return header->ym2612_clock;
        case CHIP_TYPE_YM2203: return header->ym2203_clock;
        case CHIP_TYPE_YM2151: return header->ym2151_clock;
        case CHIP_TYPE_AY8910: return header->ay8910_clock;
        case CHIP_TYPE_SN76489: return header->sn76489_clock;
        case CHIP_TYPE_YM2413: return header->ym2413_clock;
        default: return 0;
    }
}
