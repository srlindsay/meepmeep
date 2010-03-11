#include <meepmeep.h>

#define BUF_SIZE 4096

static buf_t *buf_pool = NULL;
static buf_t *shadow_buf_pool = NULL;

buf_t* buf_new(void) {
	buf_t *b;
	if (buf_pool) {
		b = buf_pool;
		buf_pool = buf_pool->next;
	} else {
		b = malloc(sizeof(buf_t));
		if (!b) { return NULL; }
		b->start = malloc(BUF_SIZE);
		if (!b->start) { free(b); return NULL;}
		b->end = b->start + BUF_SIZE;
	}
	b->shadow = 0;
	b->curr = b->curr_out = b->start;
	b->next = NULL;
	return b;
}

buf_t* buf_new_shadow(char *start, char *end) {
	buf_t *b;
	if (shadow_buf_pool) {
		b = shadow_buf_pool;
		shadow_buf_pool = shadow_buf_pool->next;
	} else {
		b = malloc(sizeof(buf_t));
		if (!b) { return NULL; }
		b->shadow = 1;
	}
	b->start = b->curr_out = start;
	b->end = b->curr = end;
	b->next = NULL;
	return b;
}

void buf_free(buf_t *b) {
	if (b->shadow) {
		b->next = shadow_buf_pool;
		shadow_buf_pool = b;
	} else {
		b->next = buf_pool;
		buf_pool = b;
	} 
}

void buf_free_chain(buf_t *b) {
	buf_t *next;
	while(b) {
		next = b->next;
		buf_free(b);
		b = next;
	}
}

int buf_chain_len(buf_t *b) {
	int len = 0;
	for ( ; b; b=b->next) {
		len += (b->end - b->start);
	}
	return len;
}

void buf_print_chain(buf_t *b) {
	while(b) {
		int len = b->end - b->start;
		printf ("%.*s", len, b->start);
		b=b->next;
	}
}

void buf_print_chain_slice(chain_slice_t *c) {
	buf_t *b;
	for (b = c->start.b; b != c->end.b; b=b->next) {
		char *start;
		int len;
		if (b == c->start.b) {
			start = c->start.loc;
		} else {
			start = b->start;
		}
		if (b == c->end.b) {
			len = c->end.loc - start;
		} else {
			len = b->end - start;
		}
		printf ("%.*s", len, start);
	}
}

