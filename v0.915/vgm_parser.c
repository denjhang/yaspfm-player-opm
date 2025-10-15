#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Structure to hold information about each VGM command
typedef struct {
    const char* name;
    uint8_t len; // Total length of the command in bytes (including the command byte itself)
} VgmCommandInfo;

// Command table based on vgmplayer_cmdhandler.cpp
const VgmCommandInfo g_cmd_info[256] = {
    {"Unknown", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 00-03
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 04-07
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 08-0B
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 0C-0F
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 10-13
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 14-17
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 18-1B
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 1C-1F
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 20-23
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 24-27
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 28-2B
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 2C-2F
    {"SN76489 #2", 2}, {"AY8910 Stereo", 2}, {"Unknown", 2}, {"Unknown", 2}, // 30-33
    {"Unknown", 2}, {"Unknown", 2}, {"Unknown", 2}, {"Unknown", 2}, // 34-37
    {"Unknown", 2}, {"Unknown", 2}, {"Unknown", 2}, {"Unknown", 2}, // 38-3B
    {"Unknown", 2}, {"Unknown", 2}, {"Unknown", 2}, {"GG Stereo #2", 2}, // 3C-3F
    {"Mikey", 3}, {"Unknown", 3}, {"Unknown", 3}, {"Unknown", 3}, // 40-43
    {"Unknown", 3}, {"Unknown", 3}, {"Unknown", 3}, {"Unknown", 3}, // 44-47
    {"Unknown", 3}, {"Unknown", 3}, {"Unknown", 3}, {"Unknown", 3}, // 48-4B
    {"Unknown", 3}, {"Unknown", 3}, {"Unknown", 3}, {"GG Stereo", 2}, // 4C-4F
    {"SN76489", 2}, {"YM2413", 3}, {"YM2612 P0", 3}, {"YM2612 P1", 3}, // 50-53
    {"YM2151", 3}, {"YM2203", 3}, {"YM2608 P0", 3}, {"YM2608 P1", 3}, // 54-57
    {"YM2610 P0", 3}, {"YM2610 P1", 3}, {"YM3812", 3}, {"YM3526", 3}, // 58-5B
    {"Y8950", 3}, {"YMZ280B", 3}, {"YMF262 P0", 3}, {"YMF262 P1", 3}, // 5C-5F
    {"Invalid", 1}, {"WAIT", 3}, {"WAIT 60Hz", 1}, {"WAIT 50Hz", 1}, // 60-63
    {"Invalid", 1}, {"Invalid", 1}, {"END", 1}, {"DATA_BLOCK", 1}, // 64-67
    {"PCM_RAM_WRITE", 12}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 68-6B
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 6C-6F
    {"WAIT n+1", 1}, {"WAIT n+1", 1}, {"WAIT n+1", 1}, {"WAIT n+1", 1}, // 70-73
    {"WAIT n+1", 1}, {"WAIT n+1", 1}, {"WAIT n+1", 1}, {"WAIT n+1", 1}, // 74-77
    {"WAIT n+1", 1}, {"WAIT n+1", 1}, {"WAIT n+1", 1}, {"WAIT n+1", 1}, // 78-7B
    {"WAIT n+1", 1}, {"WAIT n+1", 1}, {"WAIT n+1", 1}, {"WAIT n+1", 1}, // 7C-7F
    {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, // 80-83
    {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, // 84-87
    {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, // 88-8B
    {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, {"YM2612 PCM", 1}, // 8C-8F
    {"DAC_CTRL Setup", 5}, {"DAC_CTRL SetData", 5}, {"DAC_CTRL SetFreq", 6}, {"DAC_CTRL PlayLoc", 11}, // 90-93
    {"DAC_CTRL Stop", 2}, {"DAC_CTRL PlayBlk", 5}, {"Invalid", 1}, {"Invalid", 1}, // 94-97
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 98-9B
    {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, {"Invalid", 1}, // 9C-9F
    {"AY8910 #2", 3}, {"YM2413 #2", 3}, {"YM2612 #2 P0", 3}, {"YM2612 #2 P1", 3}, // A0-A3
    {"YM2151 #2", 3}, {"YM2203 #2", 3}, {"YM2608 #2 P0", 3}, {"YM2608 #2 P1", 3}, // A4-A7
    {"YM2610 #2 P0", 3}, {"YM2610 #2 P1", 3}, {"YM3812 #2", 3}, {"YM3526 #2", 3}, // A8-AB
    {"Y8950 #2", 3}, {"YMZ280B #2", 3}, {"YMF262 #2 P0", 3}, {"YMF262 #2 P1", 3}, // AC-AF
    {"RF5C68", 3}, {"RF5C164", 3}, {"PWM", 3}, {"GameBoy DMG", 3}, // B0-B3
    {"NES APU", 3}, {"MultiPCM", 3}, {"uPD7759", 3}, {"OKIM6258", 3}, // B4-B7
    {"OKIM6295", 3}, {"HuC6280", 3}, {"K053260", 3}, {"Pokey", 3}, // B8-BB
    {"WonderSwan", 3}, {"SAA1099", 3}, {"ES5506 (8-bit)", 3}, {"GA20", 3}, // BC-BF
    {"SegaPCM Mem", 4}, {"RF5C68 Mem", 4}, {"RF5C164 Mem", 4}, {"YMW Bank", 4}, // C0-C3
    {"QSound", 4}, {"SCSP", 4}, {"WSwan Mem", 4}, {"VSU-VUE", 4}, // C4-C7
    {"X1-010", 4}, {"Unknown", 4}, {"Unknown", 4}, {"Unknown", 4}, // C8-CB
    {"Unknown", 4}, {"Unknown", 4}, {"Unknown", 4}, {"Unknown", 4}, // CC-CF
    {"YMF278B", 4}, {"YMF271", 4}, {"SCC1", 4}, {"K054539", 4}, // D0-D3
    {"C140", 4}, {"ES5503", 4}, {"ES5506 (16-bit)", 4}, {"Unknown", 4}, // D4-D7
    {"Unknown", 4}, {"Unknown", 4}, {"Unknown", 4}, {"Unknown", 4}, // D8-DB
    {"Unknown", 4}, {"Unknown", 4}, {"Unknown", 4}, {"Unknown", 4}, // DC-DF
    {"YM2612 PCM Seek", 5}, {"C352", 5}, {"Unknown", 5}, {"Unknown", 5}, // E0-E3
    {"Unknown", 5}, {"Unknown", 5}, {"Unknown", 5}, {"Unknown", 5}, // E4-E7
    {"Unknown", 5}, {"Unknown", 5}, {"Unknown", 5}, {"Unknown", 5}, // E8-EB
    {"Unknown", 5}, {"Unknown", 5}, {"Unknown", 5}, {"Unknown", 5}, // EC-EF
    {"Unknown", 1}, {"Unknown", 1}, {"Unknown", 1}, {"Unknown", 1}, // F0-F3
    {"Unknown", 1}, {"Unknown", 1}, {"Unknown", 1}, {"Unknown", 1}, // F4-F7
    {"Unknown", 1}, {"Unknown", 1}, {"Unknown", 1}, {"Unknown", 1}, // F8-FB
    {"Unknown", 1}, {"Unknown", 1}, {"Unknown", 1}, {"Unknown", 1}  // FC-FF
};

uint32_t read_le32(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

uint16_t read_le16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

void print_usage(const char* prog_name) {
    fprintf(stderr, "Usage: %s <input.vgm> [-o <output.txt>] [-s <start_sec>] [-e <end_sec>]\n", prog_name);
}

int main(int argc, char* argv[]) {
    const char* in_filename = NULL;
    const char* out_filename = NULL;
    FILE* in_fp = NULL;
    FILE* out_fp = stdout;
    double start_sec = -1.0, end_sec = -1.0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) out_filename = argv[++i];
            else { print_usage(argv[0]); return 1; }
        } else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) start_sec = atof(argv[++i]);
            else { print_usage(argv[0]); return 1; }
        } else if (strcmp(argv[i], "-e") == 0) {
            if (i + 1 < argc) end_sec = atof(argv[++i]);
            else { print_usage(argv[0]); return 1; }
        } else {
            in_filename = argv[i];
        }
    }

    if (!in_filename) {
        print_usage(argv[0]);
        return 1;
    }

    in_fp = fopen(in_filename, "rb");
    if (!in_fp) {
        perror("Error opening input file");
        return 1;
    }

    if (out_filename) {
        out_fp = fopen(out_filename, "w");
        if (!out_fp) {
            perror("Error opening output file");
            fclose(in_fp);
            return 1;
        }
    }

    uint8_t header[0x100];
    if (fread(header, 1, 0x40, in_fp) != 0x40) {
        fprintf(stderr, "Failed to read VGM header.\n");
        fclose(in_fp);
        if (out_fp != stdout) fclose(out_fp);
        return 1;
    }

    uint32_t version = read_le32(header + 0x08);
    uint32_t data_offset = 0x40;
    if (version >= 0x150) {
        uint32_t vgm_data_offset_val = read_le32(header + 0x34);
        if (vgm_data_offset_val > 0) {
            data_offset = 0x34 + vgm_data_offset_val;
        }
    }
    
    fseek(in_fp, data_offset, SEEK_SET);

    fprintf(out_fp, "--- Parsing VGM: %s ---\n", in_filename);
    fprintf(out_fp, "VGM Version: 0x%X\n", version);
    fprintf(out_fp, "Data Start Offset: 0x%X\n\n", data_offset);
    fprintf(out_fp, "Time (s)  | Offset(h) | Command\n");
    fprintf(out_fp, "----------|-----------|------------------------------------------------\n");

    long current_pos = data_offset;
    uint64_t total_samples = 0;
    const double sample_rate = 44100.0;
    uint8_t cmd_buf[16];

    unsigned long long start_sample = (start_sec >= 0) ? (unsigned long long)(start_sec * sample_rate) : 0;
    unsigned long long end_sample = (end_sec >= 0) ? (unsigned long long)(end_sec * sample_rate) : -1;


    while (fread(&cmd_buf[0], 1, 1, in_fp) == 1) {
        uint8_t op = cmd_buf[0];
        const VgmCommandInfo* info = &g_cmd_info[op];
        uint8_t data_len = info->len - 1;
        
        int should_print = (total_samples >= start_sample && (end_sample == -1 || total_samples < end_sample));

        if (should_print) {
            fprintf(out_fp, "%-10.4f| %08lX  | %02X: %s", (double)total_samples / sample_rate, current_pos, op, info->name);
        }

        if (data_len > 0) {
            if (fread(&cmd_buf[1], 1, data_len, in_fp) != data_len) {
                if (should_print) fprintf(out_fp, " ... (incomplete command)\n");
                break;
            }
        }

        uint32_t wait_samples = 0;
        switch (op) {
            case 0x61: wait_samples = read_le16(&cmd_buf[1]); break;
            case 0x62: wait_samples = 735; break;
            case 0x63: wait_samples = 882; break;
            case 0x66: 
                if (should_print) fprintf(out_fp, "\n");
                goto end_loop;
            case 0x67: {
                fseek(in_fp, 2, SEEK_CUR); 
                uint32_t block_size = 0;
                fread(&block_size, 4, 1, in_fp);
                if (should_print) fprintf(out_fp, " (type: 0x%02X, size: %u)", cmd_buf[1], block_size);
                fseek(in_fp, block_size, SEEK_CUR);
                current_pos += 6 + block_size;
                data_len = 0;
                break;
            }
            case 0x50: case 0x30:
                if (should_print) fprintf(out_fp, " (val: 0x%02X)", cmd_buf[1]);
                break;
            case 0xA0: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55:
            case 0x56: case 0x57: case 0xBC:
                if (should_print) fprintf(out_fp, " (reg: 0x%02X, val: 0x%02X)", cmd_buf[1], cmd_buf[2]);
                break;
            default:
                if (op >= 0x70 && op <= 0x7F) {
                    wait_samples = (op & 0x0F) + 1;
                } else {
                    if (should_print) {
                        for (int i = 0; i < data_len; ++i) {
                            fprintf(out_fp, " %02X", cmd_buf[i + 1]);
                        }
                    }
                }
                break;
        }
        
        if (should_print) fprintf(out_fp, "\n");

        if (wait_samples > 0) {
            total_samples += wait_samples;
        }
        
        current_pos += info->len;

        if (end_sample != -1 && total_samples >= end_sample) {
            fprintf(out_fp, "\n--- Reached end time. ---\n");
            break;
        }
    }

end_loop:
    fprintf(out_fp, "\n--- Parsing complete. ---\n");

    fclose(in_fp);
    if (out_fp != stdout) {
        fclose(out_fp);
    }

    return 0;
}
