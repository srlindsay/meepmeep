#ifndef _HTTP_H
#define _HTTP_H

typedef enum {
	HTTP_OK = 200,
	HTTP_BAD_REQUEST = 400,
	HTTP_FORBIDDEN = 401,
	HTTP_INTERNAL_SERVER_ERROR = 500,
	HTTP_BAD_GATEWAY = 502
} http_response_code_t;

const char *http_get_code_text(http_response_code_t code);

#endif 


