#include <string.h>
#include <stdio.h>
#include "http-router.h"
#include "http-handlers.h"

const static route routes[] = {
	{GET, "/", handle_default},
	{GET, "/favicon.ico", handle_path},
	{GET, "/index.html", handle_default},
	{GET, "*", handle_path},

}; 

int dispatch_request(http_request *request, http_response *response) {
	if (!request) return -1;

	char *target = request->request.request_target;
	if (!target) {
		return handle_internal_server_error(request, response);
	}

	http_method method = request->request.method;

	size_t route_size = sizeof(routes) / sizeof(routes[0]);

	for (size_t i = 0; i < route_size; i++) {
		route tmp = routes[i];

		if (tmp.method == method) {
			if (strcmp(tmp.path, "*") == 0 || strcmp(tmp.path, target) == 0) {
				return tmp.handler(request, response);			
			}
		}
	}
	return handle_not_found(request, response);
}

