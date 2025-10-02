#include "error.h"
#include "play.h" // For clear_line and print_at
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

// This is the line where the info message will be displayed.
#define INFO_LINE 11
#define DEBUG_LINE 12
#define LOG_FILE "yasp_log.txt"

static log_mode_t g_log_mode = LOG_TO_CONSOLE;

void set_log_mode(log_mode_t mode) {
    g_log_mode = mode;
}

void logging(enum loglevel_t loglevel, const char *format, ...) {
    va_list arg;
    static const char *loglevel2str[] = {
        "DEBUG", "INFO", "WARN", "ERROR", "FATAL",
    };

    if (loglevel < LOG_LEVEL)
        return;

    char buffer[512];
    va_start(arg, format);
    vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    char final_message[512];
    snprintf(final_message, sizeof(final_message), ">>%s<< %s", loglevel2str[loglevel], buffer);

    // --- File Logging ---
    FILE *log_fp = fopen(LOG_FILE, "a");
    if (log_fp) {
        time_t now = time(NULL);
        char time_buf[32];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(log_fp, "[%s] %s\n", time_buf, final_message);
        fclose(log_fp);
    }

    // --- Screen Logging ---
    if (g_log_mode == LOG_TO_CONSOLE) {
        if (loglevel == LOG_LEVEL_DEBUG) {
            clear_line(DEBUG_LINE);
            print_at(0, DEBUG_LINE, final_message);
        } else if (loglevel == LOG_LEVEL_INFO) {
            clear_line(INFO_LINE);
            print_at(0, INFO_LINE, final_message);
        } else {
            clear_line(INFO_LINE);
            print_at(0, INFO_LINE, final_message);
            fprintf(stderr, "%s\n", final_message);
        }
    }
}
