#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "http-parser.h"


int parse_request_line(char *buf, request_line *line) {
	char *token = strsep(&buf, " ");
	if (!token) return -1;
	if (strcmp(token, "GET") == 0) {
		line->method = GET;
	} else if (strcmp(token, "POST") == 0) {
		line->method = POST;
	} else if(strcmp(token, "PUT") == 0) {
		line->method = PUT;
	} else {
		line->method = METHOD_NOT_SUPPORTED;
		return -1;
	}

	token = strsep(&buf, " ");
	if (!token) return -1;
	line->request_target = token;
	
	token = strsep(&buf, " ");
	if (!token) return -1;
	if (strcmp(token, "HTTP/1.1") == 0) {
		line->protocol = HTTP_1_1;
	} else {
		line->protocol = PROTOCOL_NOT_SUPPORTED;
		return -1;
	}
	return 0;
}

int parse_request_headers(char *buf, http_headers *r_headers) {
	char *p = buf;
	char *headers_end = buf + strlen(buf) ;

	if (!headers_end) {
		printf("malformed header input\n");
		return 422;
	}
	char *headers_start = p;

	while(headers_start < headers_end) {
		char *line_end = strstr(headers_start, "\r\n");
		if (!line_end) {
			return 422;
		}

		line_end[0] = '\0';

		if (r_headers->count == r_headers->capacity) {
			size_t new_capacity = r_headers->capacity * 2;
			http_header *tmp = realloc(r_headers->headers, sizeof(http_header) * new_capacity);
			if (tmp == NULL) {
				perror("realloc");
				return -1;
			}
			r_headers->headers = tmp;
			r_headers->capacity = new_capacity;
		}

		char *colon = strstr(headers_start, ": ");
		if (!colon) return 422;

		colon[0] = '\0';
		char *key = headers_start;
		char *value = colon + 2;
		http_header h;
		h.key = strdup(key);
		h.value = strdup(value);
		r_headers->headers[r_headers->count] = h;
		r_headers->count++;

		headers_start = line_end + 2;

	}

	return 0;
}

int parse_http_request(char *buf, http_request *request) {
	char *headers = strstr(buf, "\r\n");
	if (headers == NULL) {
		return -1;
	}
	headers[0] = '\0';
        headers += 2;
	char *request_body = strstr(headers, "\r\n\r\n");
	if (request_body == NULL) {
		return -1;
	}
	request_body += 4;

	char *line_buf = buf;
	request_line line;
	if (parse_request_line(line_buf, &line) == -1) {
		return -1;
	}
	request->request = line;

	size_t init_cap = 8;
	http_header *header = malloc(sizeof(http_header) * init_cap);
	http_headers r_headers;
	r_headers.count = 0;
	r_headers.capacity = init_cap;
	r_headers.headers = header;
	if (parse_request_headers(headers, &r_headers) == -1) {
		return -1;
	}
	request->headers = r_headers;

	const char* method_names[] = { "GET", "POST", "PUT", "NOT_SUPPORTED" };
	const char* protocol_names[] = { "HTTP/1.1", "NOT_SUPPORTED" };
	printf("%s %s %s\n", method_names[request->request.method], request->request.request_target, protocol_names[request->request.protocol]);

	for (size_t i = 0; i < r_headers.count; i++) {
		printf("%s: %s\n", r_headers.headers[i].key, r_headers.headers[i].value);
	}
	
	return 0;
}

void free_http_request(http_request *request) {
	if (!request || !request->headers.headers) return;

	for (size_t i = 0; i < request->headers.count; i++) {
		free(request->headers.headers[i].key);
		free(request->headers.headers[i].value);
	
	}

	free(request->headers.headers);
	request->headers.headers = NULL;
	request->headers.capacity = 0;
	request->headers.count = 0;
}

