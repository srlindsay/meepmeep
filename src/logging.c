#include <meepmeep.h>

static loglevel_t verbosity = LOG_DEBUG;

void log_write(loglevel_t level, char *fmt, ...) {
	if (verbosity < level) {
		return;
	}
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

