#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stddef.h>

#define MAX_HEADER_KEY_SIZE 256
#define MAX_HEADER_VALUE_SIZE 4096


typedef enum {
	GET,
	POST,
	PUT,
	METHOD_NOT_SUPPORTED
} http_method;

typedef enum {
	HTTP_1_1,
	PROTOCOL_NOT_SUPPORTED
} http_protocol;

typedef struct {
	http_method method;
	char *request_target;
	http_protocol protocol;
} request_line;

typedef struct {
	char *key;
	char *value;
} http_header;

typedef struct {
	http_header *headers;
	size_t count;
	size_t capacity;
} http_headers;

typedef struct {
	request_line request;
	http_headers headers;
} http_request;


int parse_http_request(char *buf, http_request *request);

void free_http_request(http_request *request);

#endif // HTTP_PARSER_H
