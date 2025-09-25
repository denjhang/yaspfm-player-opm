#include "error.h"
#include <stdarg.h>

void logging(enum loglevel_t loglevel, const char *format, ...)
{
	va_list arg;
	static const char *loglevel2str[] = {
		"DEBUG",
		"INFO",
		"WARN",
		"ERROR",
		"FATAL",
	};

	if (loglevel < LOG_LEVEL)
		return;

	fprintf(stderr, ">>%s<<\t", loglevel2str[loglevel]);

	va_start(arg, format);
	vfprintf(stderr, format, arg);
	va_end(arg);
}
