#ifndef _REQ_H
#define _REQ_H

#include <meepmeep.h>

#include <http.h>

#include <parser.h>
#include <proxy.h>

typedef struct req_st req_t;
typedef struct req_proxy_info_st req_proxy_info_t;

typedef void (req_handler_t)(req_t *r, void *data);

struct req_st {
	conn_t *conn;
	int method;
	char *uri;
	parser_t parser;
	req_t *next;
	buf_t *bufs;

	req_handler_t *inbound_proxy_data_handler;
	req_handler_t *proxy_finished_handler;
	req_proxy_info_t *proxy_info;

	unsigned req_parsed:1;	
	unsigned stream:1;
};

struct req_proxy_info_st {
	proxy_t *p;
	buf_t *chain;
	unsigned finished:1;
	req_proxy_info_t *next;
};

int req_init_inbound(conn_t *conn);
void req_read_request(conn_t *conn, void *data);

void req_init_child_proxy(req_t *r, char *uri, char *ip, short port);
void req_handoff_proxy_chain(req_t *r, proxy_t *p, buf_t *in);

const char *http_get_code_text(http_response_code_t code);

#endif

