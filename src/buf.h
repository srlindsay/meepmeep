#ifndef _BUF_H
#define _BUF_H

typedef struct buf_st buf_t;
typedef struct chain_slice_st chain_slice_t;
typedef struct chain_offset_st chain_offset_t;

struct chain_offset_st {
	buf_t *b;
	char *loc;
};

struct chain_slice_st {
	chain_offset_t start;
	chain_offset_t end;
};

struct buf_st {
	char  *start;
	char  *end;
	char  *curr;
	buf_t *next;

	int shadow:1;
};

buf_t* buf_new(void);
buf_t* buf_new_shadow(char *start, char *end);
void buf_free(buf_t *b);
void buf_free_chain(buf_t *b);
int buf_chain_len(buf_t *b);
void buf_print_chain(buf_t *b);
void buf_print_chain_slice(chain_slice_t *c);

#endif

