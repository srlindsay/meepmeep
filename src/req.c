#include <meepmeep.h>

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

	/* TODO: iterate over live proxies and close everything */

	int i;
	for (i=0; i < r->n_proxy_chains; i++) {
		buf_free_chain(r->proxy_chains[i]);
		r->proxy_chains[i] = NULL;
	}
	r->used_proxy_chains = 0;
	r->inbound_proxy_data_handler = NULL;
	r->proxy_finished_handler = NULL;

	r->conn = NULL;
	r->method = 0;
	memset(&r->parser, 0, sizeof(parser_t));
	r->proxies = NULL;
	r->req_parsed = 0;

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

void req_inbound_proxy_data_handler(req_t *r, void *data) {
	DBGTRACE();
	proxy_t *p = data;
	log_write(LOG_DEBUG, "[%s] p: %p\n", __FUNCTION__, p);
}

void req_proxy_finished_handler(req_t *r, void *data) {
	DBGTRACE();
	proxy_t *p = data;
	int id;
	id = req_get_proxy_id(r, p);
	if (id < 0) { return; }

	buf_t *chain = r->proxy_chains[id];
	r->proxy_chains[id] = NULL;
	buf_print_chain(chain);
	conn_send_and_close(r->conn, chain);
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
	int id = r->used_proxy_chains;
	r->used_proxy_chains++;
	if (r->used_proxy_chains > r->n_proxy_chains) {
		r->n_proxy_chains++;
		r->proxy_chains = realloc(r->proxy_chains, r->n_proxy_chains * sizeof(buf_t*));
		r->proxy_chains[id] = NULL;
	}
	if (r->proxies) {
		for (pl=r->proxies; pl->next; pl=pl->next){}
		pl->next = newproxy;
	} else {
		r->proxies = newproxy;
		newproxy->next = NULL;
	}
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

int req_get_proxy_id(req_t *r, proxy_t *p) {
	DBGTRACE();
	int i;
	proxy_t *plist;
	for (i = 0, plist=r->proxies; plist; i++, plist=plist->next) {
		if (plist == p) {
			return i;
		}
	}
	log_write(LOG_ERROR, "[%s] proxy(%p) not in req(%p)'s list of proxies!\n",
			__FUNCTION__, p, r);
	return -1;
}

void req_handoff_proxy_chain(req_t *r, proxy_t *p, buf_t *in) {
	DBGTRACE();
	int id;
	id = req_get_proxy_id(r, p);
	if (id < 0) { return; }
	buf_append_chain(&r->proxy_chains[id], in);
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

