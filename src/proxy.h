#ifndef _PROXY_H
#define _PROXY_H


typedef struct proxy_st proxy_t;

struct proxy_st {
	struct conn_st *parent;
};

#endif

