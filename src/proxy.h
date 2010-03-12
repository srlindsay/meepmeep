#ifndef _PROXY_H
#define _PROXY_H

#include <req.h>
#include <conn.h>

typedef struct proxy_st proxy_t;

struct req_st;

struct proxy_st {
	struct req_st *parent;
	conn_t *conn;

	const char *ip;
	short port;
	const char *uri;

	proxy_t *next;
};

proxy_t* proxy_init(const char *ip, short port, const char *uri, struct req_st *parent);

void proxy_close(proxy_t *p);

void proxy_handoff_chain(buf_t **in, proxy_t *p);

#endif

