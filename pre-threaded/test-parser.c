#include "http-parser.h"
#include <stdio.h>

int main() {
	char buf[] =
    	"GET /index.html HTTP/1.1\r\n"
    	"Host: localhost:8080\r\n"
    	"User-Agent: curl/8.7.1\r\n"
    	"Accept: */*\r\n\r\n";
    	http_request req;
    	parse_http_request(buf, &req);
	free_http_request(&req);
    	// print parsed request for testing
    	return 0;
}
