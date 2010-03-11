#include <meepmeep.h>
static conn_t *conn_pool = NULL;

conn_t* conn_new(void) {
	conn_t *c;
	if (conn_pool) {
		c = conn_pool;
		conn_pool = conn_pool->next;
	} else {
		c = malloc(sizeof(conn_t));
		if (!c) { return NULL; }
	}
	memset(c, 0, sizeof(conn_t));
	return c;
}

void conn_free(conn_t *c) {
	c->next = conn_pool;
	conn_pool = c;
}

void conn_close(conn_t *conn) {
	event_del(&conn->ev);
	if (conn->close_handler) {
		conn->close_handler(conn, NULL);
	}
	close(conn->fd);
	conn_free(conn);
}

int conn_register_events(conn_t *conn) {
	event_del(&conn->ev);
	short flags = EV_PERSIST;
	flags |= conn->want_write ? EV_WRITE : 0;
	flags |= conn->want_read ? EV_READ : 0;
	event_set(&conn->ev, conn->fd, flags, conn_handler, conn);
	if (conn->timeout) {
		conn->tv.tv_sec = conn->timeout;
		conn->tv.tv_usec = 0;
	}
	return event_add(&conn->ev, conn->timeout ? &conn->tv : NULL);
}

int buf_chain_recv(int fd, buf_t **in) {
	/* TODO: switch this to use readv */
	buf_t *b;
	int totalRead = 0;

	if (*in == NULL) {
		*in = buf_new();
	}
	b = *in;

	while (1) {
		int to_recv;
		while (1) {
			/* find the first non-full buffer in the chain */
			to_recv = b->end - b->curr;
			if (to_recv) {break;}
			if (b->next) {
				b = b->next;
			} else {
				/* all buffers full, add a new one */
				b->next = buf_new();
				b = b->next;
				break;
			}
		}

		int nb;
		nb = recv(fd, b->curr, to_recv, 0);
		if (nb < 0) {
			switch (errno) {
				case EAGAIN:
					if (totalRead == 0) {
						return -EAGAIN;
					} else {
						return totalRead;
					}
				case EINTR:
					continue;
				default:
					/* some other error */
					return -errno;
			}
		}
		if (nb == 0) {
			break;
		} else {
			totalRead += nb;
			b->curr += nb;
			if (nb < to_recv) {
				break;
			}
		}
	}
	return totalRead;
}

int buf_chain_send(int fd, buf_t **out) {
	int to_send;
	int total_sent = 0;
	int nb;
	buf_t *b;
	while (*out) {
		b = *out;
		to_send = b->end - b->curr;

		nb = send(fd, b->curr, to_send, 0);
		if (nb < 0) {
			switch(errno) {
				case EAGAIN:
					return total_sent;
				case EINTR:
					continue;
				default:
					return -errno;
			}
		}
		b->curr += nb;
		total_sent += nb;
		if (nb < to_send) {
			return total_sent;
		}
		/* sent the entire buffer, remove it from the chain */
		*out = b->next;
		buf_free(b);
	}
	return total_sent;
}

void conn_handler(int fd, short ev_type, void *data) {
	conn_t *conn = data;
	log_write (LOG_DEBUG, "data to recv on %d\n", fd);
	
	if (ev_type & EV_READ) {
		int nb;
		nb = buf_chain_recv(fd, &conn->in);
		if (nb < 0) {
			conn->error_handler(conn, EV_READ, &nb);
			conn->close_connection = 1;
		} else if (nb == 0) { 
			conn->read_closed = 1; 
			conn->want_read = 0;
			conn->update_events = 1;
		} else {
			if (conn->read_handler) {
				conn->read_handler(conn, &nb);
			}
		}
	}

	if (ev_type & EV_WRITE) {
		int nb;
		nb = buf_chain_send(fd, &conn->out);
		if (nb < 0) {
			conn->error_handler(conn, EV_WRITE, &nb);
			conn->close_connection = 1;
			conn->want_write = 0;
			conn->update_events = 1;
		}
		if (conn->write_handler) {
			conn->write_handler(conn, &nb);
		}
		if (conn->out == NULL && conn->send_and_close) {
			conn->close_connection = 1;
		}
	}

	if (ev_type & EV_TIMEOUT) {
		if (conn->timeout_handler) {
			conn->timeout_handler(conn, NULL);
		} else {
			conn->close_connection = 1;
		}
	}

	if (conn->update_events) {
		conn_register_events(conn);
		conn->update_events = 0;
	}

	if (conn->close_connection) {
		conn_close(conn);
	}
}


