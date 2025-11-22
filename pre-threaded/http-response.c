#include <stdlib.h>
#include "http-response.h"

void free_http_response(http_response *response) {
        if (!response || !response->headers.headers) return;

        for (size_t i = 0; i < response->headers.count; i++) {
                free(response->headers.headers[i].key);
                free(response->headers.headers[i].value);

        }

        free(response->headers.headers);
        response->headers.headers = NULL;
        response->headers.capacity = 0;
        response->headers.count = 0;
}
