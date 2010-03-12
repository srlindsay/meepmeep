#ifndef _PARSER_H
#define _PARSER_H

#include <str.h>

typedef struct parser_st parser_t;

typedef enum {
	PS_INIT = 0,
	PS_METHOD,
	PS_URI1,
	PS_URI2,
	PS_HTTPVER1,
	PS_HTTPVER2,
	PS_HTTPVER3,
	PS_HEADERS1,
	PS_HEADERS2,
	PS_HEADERS3,
	PS_HEADERS4,
	PS_POSTBODY
} parse_state_t;

struct parser_st {
	parse_state_t state;
	str_t method;
	str_t uri;
	str_t http_ver;
	chain_slice_t headers;
	chain_slice_t body;
};

int parse(parser_t *p, buf_t *b);
#endif

