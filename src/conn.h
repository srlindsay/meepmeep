#ifndef _CONN_H
#define _CONN_H

#include <meepmeep.h>
typedef struct conn_st conn_t;

typedef void (handler_t)(conn_t *conn, void *data);
typedef void (err_handler_t)(conn_t *conn, short ev_type, void *data);

typedef enum {
	INBOUND_HTTP = 0,
	OUTBOUND_HTTP
} conn_type_t;

struct conn_st {
	int fd;
	struct event ev;
	int timeout;
	struct timeval tv;

	buf_t *in;
	buf_t *out;
	handler_t *read_handler;
	handler_t *read_close_handler;
	handler_t *write_handler;
	handler_t *timeout_handler;
	handler_t *close_handler;
	handler_t *cleanup_handler;
	err_handler_t *error_handler;

	conn_type_t type;
	void *data;

	unsigned event_registered:1;
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

int conn_write_chain(conn_t *c, buf_t *b);

int conn_read(conn_t *c, handler_t *handler);

#endif

