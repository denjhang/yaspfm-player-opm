#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

// --- Helper Functions ---
static uint32_t read_le32(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static uint16_t read_le16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

// --- Main Parsing Logic ---
void parse_vgm_commands(FILE* fp, double start_sec, double end_sec) {
    uint8_t hdr_buf[0x40];
    if (fseek(fp, 0, SEEK_SET) != 0 || fread(hdr_buf, 1, 0x40, fp) != 0x40) {
        fprintf(stderr, "Error: Could not read header.\n");
        return;
    }

    uint32_t version = read_le32(hdr_buf + 0x08);
    uint32_t data_offset = 0;
    if (version >= 0x150) {
        data_offset = read_le32(hdr_buf + 0x34);
        data_offset = data_offset ? (0x34 + data_offset) : 0x40;
    } else {
        data_offset = 0x40;
    }

    if (fseek(fp, data_offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Could not seek to data offset 0x%X.\n", data_offset);
        return;
    }

    uint64_t total_samples = 0;
    const double vgm_sample_rate = 44100.0;
    uint64_t start_sample = (uint64_t)(start_sec * vgm_sample_rate);
    uint64_t end_sample = (uint64_t)(end_sec * vgm_sample_rate);

    printf("--- Parsing VGM from %.2fs to %.2fs ---\n", start_sec, end_sec);
    printf("Sample range: %llu to %llu\n\n", start_sample, end_sample);
    printf("%-12s %-12s %-20s\n", "Time (s)", "Samples", "Command");
    printf("--------------------------------------------------\n");

    int cmd;
    while ((cmd = fgetc(fp)) != EOF) {
        double current_time_sec = total_samples / vgm_sample_rate;
        bool should_print = (total_samples >= start_sample && total_samples <= end_sample);

        if (current_time_sec > end_sec) {
            printf("\n--- Reached end time. Stopping parse. ---\n");
            break;
        }

        switch (cmd) {
            case 0x54: { // YM2151 write
                uint8_t addr = fgetc(fp);
                uint8_t data = fgetc(fp);
                if (should_print) printf("%-12.4f %-12llu YM2151: reg 0x%02X, val 0x%02X\n", current_time_sec, total_samples, addr, data);
                break;
            }
            case 0x55: { // YM2203 write
                uint8_t addr = fgetc(fp);
                uint8_t data = fgetc(fp);
                if (should_print) printf("%-12.4f %-12llu YM2203: reg 0x%02X, val 0x%02X\n", current_time_sec, total_samples, addr, data);
                break;
            }
            case 0x56: // YM2608 port 0 write
            case 0x57: // YM2608 port 1 write
            case 0x58: // YM2610 port 0 write
            case 0x59: // YM2610 port 1 write
            case 0x5A: // YM3812 write
            case 0x5B: // YM3526 write
            case 0x5C: // Y8950 write
            case 0x5D: // YMZ280B write
            case 0x5E: // YMF262 port 0 write
            case 0x5F: { // YMF262 port 1 write
                uint8_t addr = fgetc(fp);
                uint8_t data = fgetc(fp);
                if (should_print) printf("%-12.4f %-12llu Chip 0x%02X: reg 0x%02X, val 0x%02X\n", current_time_sec, total_samples, cmd, addr, data);
                break;
            }
            case 0x61: {
                uint16_t wait = read_le16((uint8_t[]){fgetc(fp), fgetc(fp)});
                if (should_print) printf("%-12.4f %-12llu WAIT: %u samples\n", current_time_sec, total_samples, wait);
                total_samples += wait;
                break;
            }
            case 0x62: {
                if (should_print) printf("%-12.4f %-12llu WAIT: 735 samples (1/60s)\n", current_time_sec, total_samples);
                total_samples += 735;
                break;
            }
            case 0x63: {
                if (should_print) printf("%-12.4f %-12llu WAIT: 882 samples (1/50s)\n", current_time_sec, total_samples);
                total_samples += 882;
                break;
            }
            case 0x66: {
                if (should_print || total_samples < start_sample) {
                    printf("%-12.4f %-12llu END OF SOUND DATA\n", current_time_sec, total_samples);
                }
                return; // End of data
            }
            case 0x67: {
                fgetc(fp); // 0x66
                uint8_t type = fgetc(fp);
                uint32_t size = read_le32((uint8_t[]){fgetc(fp), fgetc(fp), fgetc(fp), fgetc(fp)});
                if (should_print) printf("%-12.4f %-12llu DATA BLOCK: type 0x%02X, size %u\n", current_time_sec, total_samples, type, size);
                fseek(fp, size, SEEK_CUR);
                break;
            }
            case 0xA0: { // AY8910 write
                uint8_t addr = fgetc(fp);
                uint8_t data = fgetc(fp);
                if (should_print) printf("%-12.4f %-12llu AY8910: reg 0x%02X, val 0x%02X\n", current_time_sec, total_samples, addr, data);
                break;
            }
            // Handle wait commands 0x70-0x7F
            case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
            case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
                uint16_t wait = (cmd & 0x0F) + 1;
                if (should_print) printf("%-12.4f %-12llu WAIT: %u samples\n", current_time_sec, total_samples, wait);
                total_samples += wait;
                break;
            }
            // Other dual-byte commands
            case 0x30: case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46:
            case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E:
            case 0x4F: // SN76489
            case 0xB0: // RF5C68
            case 0xB1: // RF5C164
            case 0xB2: // PWM
            case 0xB3: // YM2413
            case 0xB4: // YM2612 port 0
            case 0xB5: // YM2612 port 1
            case 0xB6: // YM2151
            case 0xB7: // YM2203
            case 0xB8: // YM2608 port 0
            case 0xB9: // YM2608 port 1
            case 0xBA: // YM2610 port 0
            case 0xBB: // YM2610 port 1
            case 0xBC: // YM3812
            case 0xBD: // YM3526
            case 0xBE: // Y8950
            case 0xBF: // YMZ280B
            case 0xC5: // SAA1099
            case 0xC6: // ES5503
            case 0xC7: // ES5506
            case 0xC8: // X1-010
            case 0xD3: // C352
            case 0xD4: // GA20
            {
                fgetc(fp); // Skip data byte
                break;
            }
            // Other triple-byte commands
            case 0xC0: // Sega PCM
            case 0xC1: // RF5C68
            case 0xC2: // RF5C164
            case 0xC3: // MultiPCM
            case 0xC4: // QSound
            case 0xD0: // YM2612
            case 0xD1: // YM2151
            case 0xD2: // C140
            {
                fgetc(fp); fgetc(fp); // Skip data bytes
                break;
            }
            // Other quad-byte commands
            case 0xE0: // PCM seek
            {
                fgetc(fp); fgetc(fp); fgetc(fp); fgetc(fp); // Skip data bytes
                break;
            }
            default:
                // Unknown or unhandled command
                break;
        }
    }
}

int main(int argc, char *argv[]) {
    char* filename = NULL;
    double start_sec = 0.0;
    double end_sec = 99999.0; // Default to a very long time

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-start") == 0 && i + 1 < argc) {
            start_sec = atof(argv[++i]);
        } else if (strcmp(argv[i], "-end") == 0 && i + 1 < argc) {
            end_sec = atof(argv[++i]);
        } else {
            filename = argv[i];
        }
    }

    if (!filename) {
        fprintf(stderr, "Usage: %s <vgm_file> [-start <seconds>] [-end <seconds>]\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    parse_vgm_commands(fp, start_sec, end_sec);

    fclose(fp);
    return 0;
}
