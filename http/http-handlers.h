#include "http-parser.h"
#include "http-response.h"

int handle_default(http_request *request, http_response *response);
int handle_path(http_request *request, http_response *response);
int handle_not_found(http_request *request, http_response *response);
int handle_internal_server_error(http_request *request, http_response *response);
