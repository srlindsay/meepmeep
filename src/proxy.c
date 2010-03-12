#include <meepmeep.h>

proxy_t* proxy_new(void) {
	return calloc(1, sizeof(proxy_t));
}

void proxy_free(proxy_t *p) {
	free(p);
}

void proxy_close(proxy_t *p) {
	DBGTRACE();
	req_t *parent;
	parent = p->parent;
	debug("[%s] p: %p, parent: %p\n", __FUNCTION__, p, parent);
	proxy_handoff_chain(&p->conn->in, p);
	req_proxy_finished(parent, p);
}

char *dummy_req = 
"GET / HTTP/1.0\r\n"
"\r\n\r\n";

void send_dummy_request(conn_t *c) {
	DBGTRACE();
	buf_t *b;
	int len = strlen(dummy_req);
	b = buf_new_shadow(dummy_req, dummy_req + len);
	conn_write_chain(c, b);
}

void proxy_close_handler(conn_t *c, void *data) {
	DBGTRACE();
	proxy_t *p = c->data;
	proxy_free(p);
}

void proxy_handoff_chain(buf_t **in, proxy_t *p) {
	DBGTRACE();
	req_t *parent;
	parent = p->parent;
	req_handoff_proxy_chain(parent, p, *in);
	*in = NULL;
}

void proxy_read_close_handler(conn_t *c, void *data) {
	DBGTRACE();
	proxy_t *p = c->data;
	proxy_close(p);
	conn_read_done(c);
	c->read_close_handler = NULL;
	c->close_connection = 1;
}

void proxy_read_handler(conn_t *c, void *data) {
	DBGTRACE();
	proxy_t *p = c->data;
	//buf_print_chain(c->in);
	proxy_handoff_chain(&p->conn->in, p);
}

void proxy_send_request(proxy_t *p) {
	DBGTRACE();
	buf_t *b = buf_new();
	const char *fmt = "%s %s HTTP/1.0\r\n\r\n\r\n";
	const char *method = "GET";
	int nb;
	int sz = b->end - b->last;
	nb = snprintf(b->last, sz, fmt, method, p->uri);
	if (nb > sz) {
		b->last = b->end;
	} else {
		b->last += (nb-1);
	}
	conn_write_chain(p->conn, b);
}

void proxy_connect_handler(conn_t *c, void *data) {
	DBGTRACE();
	proxy_t *p = c->data;
	proxy_send_request(p);
	c->read_close_handler = proxy_read_close_handler;
	c->write_handler = NULL;
	conn_read(c, proxy_read_handler);
}

proxy_t* proxy_init(const char *ip, short port, const char *uri, req_t *parent) {
	DBGTRACE();
	proxy_t *p;
	int fd;
	int res;
	struct sockaddr_in addr;
	socklen_t addr_len;

	fd = socket (AF_INET, SOCK_STREAM, 0);

	memset (&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	res = inet_aton(ip, &addr.sin_addr);
	
	if (res < 0) { perror("inet_aton"); return NULL; }
	addr_len = sizeof(struct sockaddr_in);

	res = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (res < 0) { perror("fcntl"); return NULL; }

	int rc;
	rc = connect(fd, (struct sockaddr *)&addr, addr_len);

	if (rc < 0 && errno != EINPROGRESS) {
		perror("failed to connect");
		close(fd);
		return NULL;
	}
	p = proxy_new();
	p->parent = parent;
	p->ip = ip;
	p->port = port;
	p->uri = uri;
	p->conn = conn_new();
	p->conn->type = OUTBOUND_HTTP;
	p->conn->data = p;
	p->conn->fd = fd;
	p->conn->write_handler = proxy_connect_handler;
	p->conn->close_handler = proxy_close_handler;
	p->conn->want_write = 1;
	p->conn->timeout = 30;
	conn_register_events(p->conn);
	return p;
}

