#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "http-parser.h"

typedef enum {
	OK,
	NOT_FOUND,
	INTERNAL_SERVER_ERROR
} status_code;

typedef struct {
	char *start_line;
	status_code code;
	http_headers headers;
	char *resp_body;
	size_t body_size;
} http_response;


void free_http_response(http_response *response);

#endif // HTTP_RESPONSE_H

