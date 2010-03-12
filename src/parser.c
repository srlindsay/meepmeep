#include <meepmeep.h>

int parse(parser_t *p, buf_t *b) {
	char *c;
	for (;b;b=b->next) {
		for (c=b->start;c < b->end; c++) {
			switch (p->state) {
				case PS_INIT:
					p->state = PS_METHOD;
					p->method.data = c;
				case PS_METHOD:
					if (*c == ' ') {
						p->state = PS_URI1;
						p->method.len = c - p->method.data;
					}
					break;
				case PS_URI1:
					if (*c != ' ') {
						p->uri.data = c;
						p->state = PS_URI2;
					}
					break;
				case PS_URI2:
					if (*c == ' ') {
						p->state = PS_HTTPVER1;
						p->uri.len = c - p->uri.data;
					}
					break;
				case PS_HTTPVER1:
					if (*c != ' ') {
						p->http_ver.data = c;
						p->state = PS_HTTPVER2;
					}
					break;
				case PS_HTTPVER2:
					if (*c == '\r') {
						p->state = PS_HTTPVER3;
						p->http_ver.len = c - p->uri.data;
					}
					break;
				case PS_HTTPVER3:
					if (*c == '\n') {
						p->state = PS_HEADERS1;
						p->headers.start.b = b;
						p->headers.start.loc = c;
					} else {
						return -HTTP_BAD_REQUEST;
					}
					break;
				case PS_HEADERS1:
					if (*c == '\r') {
						p->state = PS_HEADERS2;
					}
					break;
				case PS_HEADERS2:
					if (*c == '\n') {
						p->state = PS_HEADERS3;
					} else {
						p->state = PS_HEADERS1;
					}
					break;
				case PS_HEADERS3:
					if (*c == '\r') {
						p->state = PS_HEADERS4;
					} else {
						p->state = PS_HEADERS1;
					}
					break;
				case PS_HEADERS4:
					if (*c == '\n') {
						p->state = PS_POSTBODY;
						p->headers.end.b = b;
						p->headers.end.loc = c;
						p->body.start.b = b;
						p->body.start.loc = c;
					} else {
						p->state = PS_HEADERS1;
					}
					break;
				case PS_POSTBODY:
					break;
			}
		}
	}
	if (p->state == PS_POSTBODY) {
		return 0;
	} else {
		return -1;
	}
}
