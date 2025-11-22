#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "http-handlers.h"
#include "http-parser.h"
#include "constants.h"

#define TMP_BUF_SIZE 1024


char *extract_mime_type(char *file_name) {
	char *res;
	char *ext = strrchr(file_name, '.');
	if (!ext) {
		res = "text/html";
		return res;
	}
	ext++;
	if (strcmp(ext, "html") == 0) {
		res = "text/html";
		return res;
	} else if (strcmp(ext, "ico") == 0) {
		res = "image/x-icon";
		return res;
	} else {
		res = "text/html";
		return res;
	}

}

int fill_http_headers(http_response *res, struct stat *sb, char *file_name) {

	http_header content_length;
	content_length.key = strdup("Content-Length");
	char value_buf[21];
	snprintf(value_buf, sizeof(value_buf), "%lld", sb->st_size);
	content_length.value = strdup(value_buf);

	http_header content_type;
	content_type.key = strdup("Content-Type");
	char *mime_type = extract_mime_type(file_name);
	content_type.value = strdup(mime_type);


	http_header *resp_headers = malloc(sizeof(http_header) * 2);

	resp_headers[0] = content_length;
	resp_headers[1] = content_type;
	
	http_headers headers;
	headers.count = 2;
	headers.capacity = 2;
	headers.headers = resp_headers;

	res->headers = headers;
	return 0;
}

int handle_file(http_request *req, http_response *res, char *file_name) {
	int fd = open(file_name, O_RDONLY);
	if (fd == -1) {
		return 404;
	}

	struct stat sb;

	if(fstat(fd, &sb) == -1) {
		return 500;
	}

	if (!res) return 500;

	char buf[TMP_BUF_SIZE];
	int cap = TMP_BUF_SIZE;
	char *resp_body = malloc(sb.st_size + 1);

	if (!resp_body) return -1;

	ssize_t nbytes;
	int count = 0;
	while((nbytes = read(fd, buf, TMP_BUF_SIZE)) > 0) {
		memcpy(resp_body + count, buf, nbytes);
		count += nbytes;
	}

	resp_body[count +  + 11] = '\0';
	printf("count: %d\n", count);

	res->resp_body = resp_body;

	fill_http_headers(res, &sb, file_name);

	printf("default done\n");
	close(fd);
	return 0;
}

/**
 *
 * Always returns an index.html file
 */
int handle_default(http_request *req, http_response *res) {
	printf("handle default\n");
	handle_file(req, res, INDEX_FILE);
	res->code = 200;
	res->start_line = "HTTP/1.1 200 OK";
	return 0;
}

int handle_path(http_request *req, http_response *res) {
	printf("handle path\n");
	
	// Guard against path traversal
	if (strstr(req->request.request_target, "..") != NULL) {
		return handle_not_found(req, res);
	}

	char safe_path[SAFE_PATH_MAX];
	snprintf(safe_path, SAFE_PATH_MAX, "%s/%s", HTTP_STATIC_DIR, req->request.request_target);

	int status_code = handle_file(req, res, safe_path);
	if (status_code == 404) {
		return handle_not_found(req, res);
	}
	if (status_code == 500) {
		return handle_internal_server_error(req, res);
	}
	res->code = 200;
	res->start_line = "HTTP/1.1 200 OK";
	return 0;
}

int handle_not_found(http_request *req, http_response *res) {
	printf("handle not found\n");
	handle_file(req, res, NOTFOUND_FILE);
	res->code = 404;
	res->start_line = "HTTP/1.1 404 Not Found";
	return 0;
}

int handle_internal_server_error(http_request *req, http_response *res) {
	printf("internal server error\n");
	handle_file(req, res, SERVER_ERROR_FILE);
	res->code = 500;
	res->start_line = "HTTP/1.1 500 Internal Server Error";
	return 0;
}

