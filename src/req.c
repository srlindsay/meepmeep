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
	req_t *r;
	if (req_pool) {
		r = req_pool;
		req_pool = req_pool->next;
		memset(r, 0, sizeof(req_t));
	} else {
		r = calloc(1, sizeof(req_t));
	}
	return r;
}

void req_free(req_t *r) {
	r->next = req_pool;
	req_pool = r;
}

void req_close_handler(conn_t *c, void *data) {
	printf ("[%s]\n", __FUNCTION__);
	req_free(c->req);
}

void req_read_request(conn_t *conn, void *data) {
	printf ("[%s] read:", __FUNCTION__);
	buf_print_chain(conn->in);
	conn->req = req_new();
	conn->close_handler = req_close_handler;

	if (parse(&conn->req->parser, conn->in) < 0) {
		/* need more stuff */
	} else {
		printf ("[%s] finished parsing\n", __FUNCTION__);
		conn->req->bufs = conn->in;
		conn->in = NULL;
		req_send_test_response(conn);
	}
}

