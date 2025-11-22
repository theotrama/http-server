#include "http-parser.h"
#include "http-response.h"

typedef struct {
    http_method method;
    const char *path;
    int (*handler)(http_request*, http_response*);
} route;

int dispatch_request(http_request *request, http_response *response);
