#include <meepmeep.h>

const char *http_get_code_text(http_response_code_t code) {
	const char *s = NULL;
	switch (code) {
		case HTTP_OK: s = "OK"; break;
		case HTTP_BAD_REQUEST: s = "Bad Request"; break;
		case HTTP_FORBIDDEN: s = "Forbidden"; break;
		case HTTP_INTERNAL_SERVER_ERROR: s = "Internal Server Error"; break;
		case HTTP_BAD_GATEWAY: s = "Bad Gateway"; break;
		default: s = "Internal Server Error"; break;
	}
	return s;
}


