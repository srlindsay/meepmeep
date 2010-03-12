#include <meepmeep.h>

int setup_socket(const char *ip, const short port) {
	int sock, res;
	struct sockaddr_in addr;
	socklen_t addr_len;

	sock = socket (AF_INET, SOCK_STREAM, 0);

	int opt = 1;
	socklen_t optlen = sizeof(int);
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, optlen);

	memset (&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	res = inet_aton(ip, &addr.sin_addr);
	
	if (res < 0) { perror("inet_aton"); return -1; }
	addr_len = sizeof(struct sockaddr_in);

	res = bind (sock, (struct sockaddr *) &addr, addr_len);
	if (res < 0) { perror("bind"); return -1; }

	res = listen(sock, 255);
	if (res < 0) { perror("listen"); return -1; }

	res = fcntl(sock, F_SETFL, O_NONBLOCK);
	if (res < 0) { perror("fcntl"); return -1; }
	
	return sock;
}

void accept_handler(int fd, short ev_type, void *data) {
	int newfd;
	int res;
	struct sockaddr_in addr;
	socklen_t addr_len;
	char buf[1024];
	size_t nb;
	conn_t *conn;

	newfd = accept(fd, (struct sockaddr *)&addr, &addr_len);
	if (newfd < 0) {
		/* another process beat us to the accept? */
		return;
	}
	
	fcntl(newfd, F_SETFL, O_NONBLOCK);

	conn = conn_new();
	conn->fd = newfd;

	if (req_init_inbound(conn) < 0) {
		conn_free(conn);
	}
}

struct event* setup_accept_event(int listen_fd) {
	int rc;

	struct event *accept_ev;

	accept_ev = (struct event *)calloc(1, sizeof(struct event));

	if (!accept_ev) return;

	event_set(accept_ev, listen_fd, EV_READ | EV_PERSIST, accept_handler, NULL);

	rc = event_add(accept_ev, NULL);
	if (rc != 0) {
		log_write (0, "[%s] error...\n", __FUNCTION__);
	}
	return accept_ev;
}

int main (int argc, char **argv) {
	int listen_fd = setup_socket("127.0.0.1", 8080);
	if (listen_fd < 0) {
		return 1;
	}
	log_write (0, "listen_fd: %d\n", listen_fd);

	event_init();
	struct event *accept_ev = setup_accept_event(listen_fd);

	event_dispatch();
	return 0;
}

