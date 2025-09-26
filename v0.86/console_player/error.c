#include "error.h"
#include "play.h" // For clear_line and print_at
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

// This is the line where the info message will be displayed.
#define INFO_LINE 11

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

    // For INFO level, print at the bottom of the UI
    if (loglevel == INFO) {
        char final_message[512];
        snprintf(final_message, sizeof(final_message), ">>INFO<< %s", buffer);
        clear_line(INFO_LINE);
        print_at(0, INFO_LINE, final_message);
    } else {
        // For other levels, print to stderr as before, but also on the info line
        // so it's visible when the UI is active.
        char final_message[512];
        snprintf(final_message, sizeof(final_message), ">>%s<< %s", loglevel2str[loglevel], buffer);
        clear_line(INFO_LINE);
        print_at(0, INFO_LINE, final_message);
        
        // Also print to stderr for logging purposes
        fprintf(stderr, "%s\n", final_message);
    }
}
