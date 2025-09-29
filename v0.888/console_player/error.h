#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum loglevel_t {
	DEBUG = 0,
	INFO,
	WARN,
	LOG_ERROR,
	LOG_FATAL,
	LOG_LEVEL = DEBUG,
};

typedef enum {
    LOG_TO_CONSOLE,
    LOG_TO_FILE
} log_mode_t;

void set_log_mode(log_mode_t mode);
void logging(enum loglevel_t loglevel, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* ERROR_H */
