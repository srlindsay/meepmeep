#include <meepmeep.h>

#define FINISHED_PROXY ((proxy_t*)-1)

char *test_response = 
"HTTP/1.1 200 OK\r\n"
"Server: meepmeep 0.1\r\n"
"Content-Type: text/plain\r\n\r\n"
"Some random stuff!\r\n";

void req_send_test_response(conn_t *c) {
	buf_t *b;
	int len = strlen(test_response);
	b = buf_new_shadow(test_response, test_response + len );
	c->out = b;
	c->want_write = 1;
	c->update_events = 1;
	c->send_and_close = 1;
}

req_proxy_info_t *req_proxy_info_new(void) {
	return calloc(1, sizeof(req_proxy_info_t));
}

void req_proxy_info_free(req_proxy_info_t* rpi) {
	req_proxy_info_t *next;
	next = rpi->next;
	buf_free_chain(rpi->chain);
	free(rpi);
	if (next) {
		req_proxy_info_free(next);
	}
}

static req_t *req_pool;

req_t* req_new(void) {
	DBGTRACE();
	req_t *r;
	if (req_pool) {
		r = req_pool;
		req_pool = req_pool->next;
	} else {
		r = calloc(1, sizeof(req_t));
	}
	return r;
}

void req_free(req_t *r) {
	DBGTRACE();
	buf_free_chain(r->bufs);
	r->bufs = NULL;

	req_proxy_info_free(r->proxy_info);
	r->proxy_info = NULL;
	r->inbound_proxy_data_handler = NULL;
	r->proxy_finished_handler = NULL;

	r->conn = NULL;
	r->method = 0;
	memset(&r->parser, 0, sizeof(parser_t));
	r->req_parsed = 0;
	r->stream = 0;

	r->next = req_pool;
	req_pool = r;
}


void req_close_handler(conn_t *c, void *data) {
	DBGTRACE();
	req_t *r = c->data;
	req_free(r);
}

void req_cleanup_handler(conn_t *c, void *data) {
	DBGTRACE();
	debug("oh hai");
	req_free(c->data);
}

void req_read_close_handler(conn_t *conn, void *data) {
	DBGTRACE();
	log_write(LOG_DEBUG, "[%s] conn: %p, data: %p\n", __FUNCTION__, conn, data);
	conn->close_connection = 1;
}

static req_proxy_info_t* get_proxy_info(req_t *r, proxy_t *p) {
	req_proxy_info_t *rpi;
	for (rpi=r->proxy_info; rpi; rpi=rpi->next) {
		if (rpi->p == p) {
			return rpi;
		}
	}

	log_write(LOG_ERROR, "[%s] r: %p, p: %p, no proxy info found\n",
			__FUNCTION__, r, p);
	return NULL;
}

static buf_t** get_proxy_chain(req_t *r, proxy_t *p) {
	req_proxy_info_t *rpi;
	rpi = get_proxy_info(r, p);
	if (rpi) {
		return &rpi->chain;
	}
	return NULL; 
}

void remove_content_length(buf_t *b) {
	char *cl;
	for (cl = b->first; cl < b->last; cl++) {
		if (*cl == 'C') {
			if (strncmp("Content-Length", cl, sizeof("Content-Length")-1) == 0) {
				for (;cl < b->last, *cl != '\r'; cl++) {
					*cl = ' ';
				}
			}
		}
	}
}

void req_inbound_proxy_data_handler(req_t *r, void *data) {
	DBGTRACE();
	proxy_t *p = data;
	log_write(LOG_DEBUG, "[%s] p: %p\n", __FUNCTION__, p);

	if (r->stream) {
		buf_t **chain = get_proxy_chain(r, p);
		if (!chain) { return; }
		remove_content_length(*chain);
		conn_write_chain(r->conn, *chain);
		*chain = NULL;
	}
}

void req_proxy_finished_handler(req_t *r, void *data) {
	DBGTRACE();
	proxy_t *p = data;
	req_proxy_info_t *rpi;
	rpi = get_proxy_info(r, p);

	rpi->finished = 1;

	for (rpi = r->proxy_info; rpi; rpi=rpi->next) {
		if (!rpi->finished) {
			return;
		}
	}

	conn_send_and_close(r->conn, NULL);
}

void req_init_proxy(req_t *r) {
	DBGTRACE();
	proxy_t *newproxy, *pl;
	const char *uri = r->uri;
	debug("[%s] uri: %s\n", __FUNCTION__, r->uri);
	newproxy = proxy_init("127.0.0.1", 80, uri, r);
	if (!newproxy) {
		log_write(LOG_ERROR, "[%s] failed to initialize proxy\n", __FUNCTION__);
		return;
	}
	r->inbound_proxy_data_handler = req_inbound_proxy_data_handler;
	r->proxy_finished_handler = req_proxy_finished_handler;

	req_proxy_info_t *rpi;
	rpi = req_proxy_info_new();
	if (!r->proxy_info) {
		r->proxy_info = rpi;
	} else {
		req_proxy_info_t *iter;
		for (iter = r->proxy_info; iter->next; iter=iter->next){}
		iter->next = rpi;
	}
	rpi->p = newproxy;
}

void req_read_request(conn_t *conn, void *data) {
	DBGTRACE();
	req_t *r;
	r = conn->data;
	buf_print_chain(conn->in);

	if (!r->req_parsed) {
		int res;
		res = parse(&r->parser, conn->in);
		switch (res) {
			case -1:
				/* need more stuff */
				return;
			case 0:
				/* parsing finished successfully */
				r->req_parsed = 1;
				break;
			default:
				/* some error condition */
				conn_send_response(conn, res, NULL);
				conn->close_connection = 1;
				return;
		}
	}

	printf ("[%s] finished parsing\n", __FUNCTION__);
	r->bufs = conn->in;
	r->uri = r->parser.uri.data;
	r->uri[r->parser.uri.len] = '\0';
	conn->in = NULL;
	conn_read_done(conn);
	req_init_proxy(r);
	req_init_proxy(r);
}

void req_read_init_request(conn_t *conn, void *data) {
	DBGTRACE();
	req_t *r;
	conn->data = r = req_new();
	if (!r) {
		log_write(LOG_ERROR, "[%s] failed to allocate request\n", __FUNCTION__);
		conn_send_response(conn, HTTP_INTERNAL_SERVER_ERROR, NULL);
		return;
	}
	r->conn = conn;
	r->stream = 1;
	conn->read_handler = req_read_request;
	req_read_request(conn, data);
}

int req_init_inbound(conn_t *c) {
	DBGTRACE();
	c->read_handler = req_read_init_request;
	c->close_handler = req_close_handler;
	c->timeout = 30;
	c->want_read = 1;
	if (conn_register_events(c) < 0) {
		log_write (0, "error trying to add event for fd: %d\n", c->fd);
		return -1;
	}
	return 0;
}

void req_handoff_proxy_chain(req_t *r, proxy_t *p, buf_t *in) {
	DBGTRACE();
	req_proxy_info_t *rpi;
	rpi = get_proxy_info(r, p);
	if (!rpi) return;
	buf_append_chain(&rpi->chain, in);
	if (r->inbound_proxy_data_handler) {
		r->inbound_proxy_data_handler(r, p);
	}
}

void req_proxy_finished(req_t *r, proxy_t *p) {
	DBGTRACE();
	if (r->proxy_finished_handler) {
		r->proxy_finished_handler(r, p);
	}
}

