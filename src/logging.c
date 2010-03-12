#include <meepmeep.h>

static loglevel_t verbosity = LOG_DEBUG;

void log_write(loglevel_t level, char *fmt, ...) {
	if (level < verbosity) {
		return;
	}
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void debug (char *fmt, ...) {
	if (LOG_DEBUG < verbosity) {
		return;
	}
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

