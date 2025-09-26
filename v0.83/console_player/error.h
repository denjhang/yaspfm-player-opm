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

void logging(enum loglevel_t loglevel, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* ERROR_H */
