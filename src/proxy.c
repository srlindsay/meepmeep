#include <meepmeep.h>

proxy_t* proxy_new(void) {
	return calloc(1, sizeof(proxy_t));
}

void proxy_free(proxy_t *p) {
	free(p);
}

char *dummy_req = 
"GET / HTTP/1.0\r\n"
"\r\n\r\n";

void send_dummy_request(conn_t *c) {
	buf_t *b;
	int len = strlen(dummy_req);
	b = buf_new_shadow(dummy_req, dummy_req + len);
	c->out = b;
	c->want_write = 1;
	c->want_read = 1;
	c->update_events = 1;
}

void proxy_read_handler(conn_t *c, void *data) {
	printf ("[%s]\n", __FUNCTION__);
	if (c->read_closed) {
		proxy_t *p;
		p = c->data;
		printf ("[%s] p->parent: %p\n", __FUNCTION__, p->parent);
		p->parent->out = c->in;
		c->in = NULL;
		p->parent->want_write = 1;
		p->parent->send_and_close = 1;
		conn_register_events(p->parent);
		c->close_connection = 1;
	} else {
		buf_print_chain(c->in);
	}
}

void proxy_connect_handler(conn_t *c, void *data) {
	printf ("[%s]\n", __FUNCTION__);
	send_dummy_request(c);
	c->write_handler = NULL;
	c->read_handler = proxy_read_handler;
}

int proxy_init(const char *ip, short port, conn_t *parent) {
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
	
	if (res < 0) { perror("inet_aton"); return -1; }
	addr_len = sizeof(struct sockaddr_in);

	res = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (res < 0) { perror("fcntl"); return -1; }

	int rc;
	rc = connect(fd, (struct sockaddr *)&addr, addr_len);

	if (rc < 0 && errno != EINPROGRESS) {
		perror("failed to connect");
		close(fd);
		return -errno;
	}
	p = proxy_new();
	p->parent = parent;

	conn_t *conn = conn_new();
	conn->data = p;
	conn->fd = fd;
	conn->write_handler = proxy_connect_handler;
	conn->want_write = 1;
	conn->timeout = 30;
	conn_register_events(conn);
}

