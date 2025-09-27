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

extern volatile int g_flush_mode;
extern volatile int g_vgm_loop_count;
extern int g_current_song_total_samples;

// --- Conversion State ---
bool g_opn_to_opm_conversion_enabled = false;
bool g_ay_to_opm_conversion_enabled = false;
bool g_sn_to_ay_conversion_enabled = false;
volatile bool g_is_playing_from_cache = false;
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

static bool vgm_convert_and_cache_opn_to_opm_from_mem(const uint8_t* vgm_data, size_t vgm_data_size, const vgm_header_t* original_header);
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

void vgm_play(FILE *input_fp, const char *filename, const char *cache_filename) {
    (void)filename;
    extern volatile bool g_next_track_flag, g_prev_track_flag, g_quit_flag, g_stop_current_song;
    
    FILE* current_fp = input_fp;
    bool is_realtime_conversion = false;

    // Always parse the header of the original file first to get original chip type and GD3.
    if (!vgm_parse_header(current_fp, &g_vgm_header)) {
        g_current_song_total_samples = 0;
        return; // Can't even parse original, bail out.
    }
    g_current_song_total_samples = g_vgm_header.total_samples;
    g_original_vgm_chip_type = get_primary_chip_from_header(&g_vgm_header);
    g_vgm_chip_type = g_original_vgm_chip_type;
    
    bool needs_opn_conversion = (g_vgm_chip_type == CHIP_TYPE_YM2612 || g_vgm_chip_type == CHIP_TYPE_YM2203 || g_vgm_chip_type == CHIP_TYPE_YM2608);
    bool opm_available = (get_slot_for_chip(CHIP_TYPE_YM2151) != 0xFF);

    g_is_playing_from_cache = false;
    g_opn_to_opm_conversion_enabled = false;

    if (needs_opn_conversion && opm_available) {
        FILE* cache_fp_read = fopen(cache_filename, "rb");
        if (cache_fp_read) {
            // --- CACHE EXISTS ---
            logging(INFO, "Found cache file: %s. Playing from cache.", cache_filename);
            fclose(current_fp); // Close original file
            current_fp = cache_fp_read;
            g_is_playing_from_cache = true;
            if (!vgm_parse_header(current_fp, &g_vgm_header)) { 
                fclose(current_fp); 
                g_current_song_total_samples = 0;
                return; 
            }
            g_current_song_total_samples = g_vgm_header.total_samples;
            g_vgm_chip_type = get_primary_chip_from_header(&g_vgm_header);
        } else {
            // --- CACHE DOES NOT EXIST, CREATE IT ---
            logging(INFO, "Cache not found. Converting %s to OPM...", chip_type_to_string(g_vgm_chip_type));
            
            // 1. Read entire original file into memory
            fseek(input_fp, 0, SEEK_END);
            long original_file_size = ftell(input_fp);
            fseek(input_fp, 0, SEEK_SET);
            uint8_t* original_file_data = malloc(original_file_size);
            if (!original_file_data || fread(original_file_data, 1, original_file_size, input_fp) != original_file_size) {
                logging(LOG_ERROR, "Failed to read original VGM file into memory.");
                if (original_file_data) free(original_file_data);
                return;
            }
            fclose(input_fp); // Close original file, we have it in memory now

            // 2. Open cache file for writing
            g_cache_fp = fopen(cache_filename, "wb");
            if (!g_cache_fp) {
                logging(LOG_ERROR, "Failed to open cache file for writing: %s", cache_filename);
                free(original_file_data);
                return;
            }

            // 3. Write placeholder header and convert data
            uint8_t header_buf[0x100] = {0};
            fwrite(header_buf, 1, 0x100, g_cache_fp);
            long data_start_offset = ftell(g_cache_fp);

            const uint8_t* vgm_data_ptr = original_file_data + g_vgm_header.vgm_data_offset;
            size_t vgm_data_size = (g_vgm_header.eof_offset + 4) - g_vgm_header.vgm_data_offset;
            vgm_convert_and_cache_opn_to_opm_from_mem(vgm_data_ptr, vgm_data_size, &g_vgm_header);

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
            write_le32(header_buf + 0x1C, 0); // No loop in converted file
            write_le32(header_buf + 0x20, 0); // No loop in converted file
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
            if (!current_fp) { logging(LOG_ERROR, "Failed to open newly created cache file!"); return; }
            g_is_playing_from_cache = true;
            if (!vgm_parse_header(current_fp, &g_vgm_header)) { fclose(current_fp); return; }
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
}

DWORD WINAPI vgm_player_thread(LPVOID lpParam) {
    FILE* input_fp = (FILE*)lpParam;
    int vgm_wait1 = VGM_DEFAULT_WAIT1, vgm_wait2 = VGM_DEFAULT_WAIT2;
    int loop_counter = 1;
    extern volatile bool g_is_paused, g_stop_current_song;
    extern volatile bool g_next_track_flag, g_prev_track_flag, g_quit_flag;
    extern volatile double g_speed_multiplier;

    while (g_is_playing && !g_next_track_flag && !g_prev_track_flag && !g_stop_current_song && !g_quit_flag) {
        while (g_is_paused && !g_next_track_flag && !g_prev_track_flag && !g_stop_current_song && !g_quit_flag) {
            yasp_usleep(100000);
        }
        int samples = vgm_process_command(input_fp, &vgm_wait1, &vgm_wait2, &loop_counter);
        if (samples > 0) {
            yasp_usleep((uint64_t)samples * 1000000 / (VGM_SAMPLE_RATE * g_speed_multiplier));
        }
    }
    spfm_flush();
    g_is_playing = false;
    return 0;
}

// This new version operates on a memory buffer instead of a file stream to avoid pointer confusion.
static bool vgm_convert_and_cache_opn_to_opm_from_mem(const uint8_t* vgm_data, size_t vgm_data_size, const vgm_header_t* original_header) {
    chip_type_t original_chip_type = get_primary_chip_from_header(original_header);
    uint32_t original_clock = get_clock_from_header(original_header, original_chip_type);
    opn_to_opm_init(original_chip_type, original_clock, vgm_cache_opm_writer);

    size_t pos = 0;
    while (pos < vgm_data_size) {
        uint8_t op = vgm_data[pos++];
        switch (op) {
            case 0x52: case 0x53: case 0x55: case 0x56: case 0x57: {
                if (pos + 2 > vgm_data_size) goto end_convert_loop;
                uint8_t addr = vgm_data[pos++];
                uint8_t data = vgm_data[pos++];
                opn_to_opm_write_reg(addr, data, (op == 0x53 || op == 0x57));
                break;
            }
            case 0x61: {
                if (pos + 2 > vgm_data_size) goto end_convert_loop;
                uint16_t wait = read_le16(&vgm_data[pos]);
                pos += 2;
                fputc(op, g_cache_fp);
                fwrite(&wait, 2, 1, g_cache_fp);
                break;
            }
            case 0x62: case 0x63: case 0x66:
                fputc(op, g_cache_fp);
                break;
            case 0x67: // Data block
                pos += 6; // Skip data block command
                break;
            case 0xE0: // PCM data
                pos += 4; // Skip PCM data command
                break;
            default:
                if (op >= 0x70 && op <= 0x7F) fputc(op, g_cache_fp);
                else if (op >= 0x80 && op <= 0x8F) pos++; // YM2612 port 0/1 address setting + pcm data
                break;
        }
    }
end_convert_loop:;
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
