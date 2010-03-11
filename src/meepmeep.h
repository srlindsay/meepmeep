#ifndef _MEEPMEEP_H
#define _MEEPMEEP_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <event.h>

#include <buf.h>
#include <logging.h>
#include <str.h>
#include <parser.h>
#include <proxy.h>

typedef struct conn_st conn_t;
typedef struct req_st req_t;

typedef void (handler_t)(conn_t *conn, void *data);
typedef void (err_handler_t)(conn_t *conn, short ev_type, void *data);

struct req_st {
	int method;
	parser_t parser;
	req_t *next;
	buf_t *bufs;
};

struct conn_st {
	int fd;
	struct event ev;
	int timeout;
	struct timeval tv;

	buf_t *in;
	buf_t *out;
	handler_t *read_handler;
	handler_t *write_handler;
	handler_t *timeout_handler;
	handler_t *close_handler;
	err_handler_t *error_handler;

	req_t *req;
	void *data;

	unsigned read_closed:1;
	unsigned close_connection:1;

	unsigned want_read:1;
	unsigned want_write:1;
	unsigned update_events:1;
	unsigned send_and_close:1;

	conn_t *next;
};


void conn_handler(int fd, short ev_type, void *data);

conn_t* conn_new(void);
void conn_free(conn_t *conn);
int conn_register_events(conn_t *conn);

void req_read_request(conn_t *conn, void *data);
#endif
