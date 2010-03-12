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
	if (conn->event_registered) {
		event_del(&conn->ev);
		conn->event_registered = 0;
	}
	if (conn->close_handler) {
		conn->close_handler(conn, NULL);
	}
	close(conn->fd);
	conn_free(conn);
}

int conn_register_events(conn_t *conn) {
	if (conn->event_registered) {
		event_del(&conn->ev);
	}
	short flags = EV_PERSIST;
	flags |= conn->want_write ? EV_WRITE : 0;
	flags |= conn->want_read ? EV_READ : 0;
	event_set(&conn->ev, conn->fd, flags, conn_handler, conn);
	if (conn->timeout) {
		conn->tv.tv_sec = conn->timeout;
		conn->tv.tv_usec = 0;
	}
	conn->event_registered = 1;
	return event_add(&conn->ev, conn->timeout ? &conn->tv : NULL);
}

int conn_write_chain(conn_t *c, buf_t *b) {
	buf_append_chain(&c->out, b);
	if (!c->want_write) {
		c->want_write = 1;
		conn_register_events(c);
	}
	return 0;
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
			to_recv = b->end - b->last;
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
		nb = recv(fd, b->last, to_recv, 0);
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
			b->last += nb;
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
		to_send = b->last - b->first;

		debug("[%s] fd: %d, b: %p, b->first: %p, b->last: %p, to_send: %d\n", 
				__FUNCTION__, fd, b, b->first, b->last, to_send);

		nb = send(fd, b->first, to_send, 0);
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
		b->first += nb;
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
	debug("[%s] data to recv on %d\n", __FUNCTION__, fd);
	
	if (ev_type & EV_READ) {
		int nb;
		nb = buf_chain_recv(fd, &conn->in);
		if (nb < 0) {
			conn->error_handler(conn, EV_READ, &nb);
			goto close_connection;
		} else if (nb == 0) { 
			if (conn->read_close_handler) {
				conn->read_close_handler(conn, NULL);
			}
			conn->want_read = 0;
			conn->update_events = 1;
			if (conn->close_connection) {
				goto close_connection;
			}
		} else {
			if (conn->read_handler) {
				conn->read_handler(conn, NULL);
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
		if (conn->out == NULL) {
			conn->want_write = 0;
			conn->update_events = 1;
			if (conn->send_and_close) {
				goto close_connection;
			}
		}
		if (conn->write_handler) {
			conn->write_handler(conn, &nb);
		}
	}

	if (ev_type & EV_TIMEOUT) {
		if (conn->timeout_handler) {
			conn->timeout_handler(conn, NULL);
		} else {
			log_write(LOG_ERROR, "[%s] timeout on conn %p, but no timeout handler\n",
					__FUNCTION__, conn);
			goto close_connection;
		}
	}

	if (conn->close_connection) goto close_connection;

	if (conn->update_events) {
		conn_register_events(conn);
		conn->update_events = 0;
	}

	return;

close_connection:
	conn_close(conn);
}

void conn_dummy_handler(conn_t *c, void *data) {
	log_write(LOG_ERROR, "[%s] c: %p\n", __FUNCTION__, c);
}

int conn_send_response(conn_t *c, http_response_code_t code, buf_t *out) {
	buf_t *b;
	b = buf_new();
	int sz = b->end - b->last;

	const char *code_text = http_get_code_text(code);

	int nb = snprintf(b->last, sz, "HTTP/1.1 %s %d\r\n", code_text, code);
	if (nb > sz) {
		log_write(LOG_ERROR, "[%s] buffer too small for response line!\n", __FUNCTION__);
		b->last = b->end;
	} else {
		b->last += nb;
	}

	if (out) {
		b->next = out;
	}

	conn_send_and_close(c, b);
	return 0;
}

int conn_send_and_close(conn_t *c, buf_t *out) {
	conn_write_chain(c, out);
	c->read_handler = conn_dummy_handler;
	c->read_close_handler = conn_dummy_handler;
	c->send_and_close = 1;
	return 0;
}

int conn_read(conn_t *conn, handler_t *handler) {
	conn->read_handler = handler;
	if (!conn->want_read) {
		conn->want_read = 1;
		conn_register_events(conn);
	}
	return 0;
}

int conn_read_done(conn_t *conn) {
	conn->read_handler = NULL;
	if (conn->want_read) {
		conn->want_read = 0;
		conn_register_events(conn);
	}
}

