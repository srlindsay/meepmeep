#ifndef _LOGGING_H
#define _LOGGING_H

typedef enum {
	LOG_DEBUG = 0,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR
} loglevel_t;

void log_write(loglevel_t level, char *fmt, ...);

void debug(char *fmt, ...);

#define DBGTRACE() log_write(LOG_DEBUG, "[%s]\n", __FUNCTION__)

#endif

