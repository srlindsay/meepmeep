#ifndef _LOGGING_H
#define _LOGGING_H

typedef enum {
	LOG_DEBUG = 0,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR
} loglevel_t;

void log_write(loglevel_t level, char *fmt, ...);

#endif

