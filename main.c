#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <libc.h>
#include <sys/poll.h>
#include <stdbool.h>

#define BUF_SIZE 1024
#define BACKLOG 10
#define PORT "8080"
#define MAX_CLIENTS 1000


enum HttpMethod {
    GET, POST
};

enum HttpVersion {
    VERSION_1_1, UNDEFINED
};

enum HttpStatus {
    NOT_FOUND, OK
};

struct http_request {
    enum HttpMethod http_method;
    char *request_path;
    enum HttpVersion http_version;
};

struct http_response {
    enum HttpMethod http_method;
    enum HttpVersion http_version;
    enum HttpStatus httpStatus;
    unsigned long content_length;
};

struct http_request parse_http_request(char *buffer, ssize_t data_size) {
    printf("Parsing http request.\n");

    enum HttpMethod httpMethod;
    char *token = strtok(buffer, " ");
    if (strcmp(token, "GET") == 0) {
        httpMethod = GET;
    } else {
        httpMethod = POST;
    }
    char *requested_path = strtok(NULL, " ");
    if (requested_path[0] == '/') requested_path++;
    char *directory_escape = "../";
    char *result = malloc(strlen("../") + strlen(requested_path) + 1);
    strcpy(result, directory_escape);
    strcat(result, requested_path);

    char *http_version = strtok(NULL, "\r\n");
    enum HttpVersion httpVersion;
    if (strcmp(http_version, "HTTP/1.1") == 0) {
        httpVersion = VERSION_1_1;
    } else {
        httpVersion = UNDEFINED;
    }

    struct http_request request = {httpMethod, result, httpVersion};

    printf("%i\n", request.http_method);
    printf("%s\n", request.request_path);
    printf("%i\n", request.http_version);


    char *strtok_return;
    while ((strtok_return = strtok(NULL, "\r\n")) != NULL) {
        printf("%s\n", strtok_return);
    }

    printf("Parsing done.\n");
    return request;
}

char *read_complete_file(char *file_name) {
    FILE *fp = fopen(file_name, "rb");
    if (fp == NULL) {
        printf("Error: Failed to open file '%s'.\n", file_name);
        return "";
    }

    fseek(fp, 0L, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    char *result = malloc(file_size);
    fread(result, file_size, 1, fp);

    return result;
}

char *response_as_string(char *file_content, char *status_code, char *status_response) {
    char content_length_header[100];
    sprintf(content_length_header, "Content-Length: %lu", strlen(file_content));
    char *result = malloc(
            strlen("HTTP/1.1") + strlen(" ") + strlen(status_code) + strlen(" ") + strlen(status_response) +
            strlen("\r\n") + strlen(content_length_header) + strlen("\r\n\r\n") +
            strlen(file_content) + 1);

    strcpy(result, "HTTP/1.1");
    strcat(result, " ");
    strcat(result, status_code);
    strcat(result, " ");
    strcat(result, status_response);
    strcat(result, "\r\n");
    strcat(result, content_length_header);
    strcat(result, "\r\n\r\n");
    strcat(result, file_content);
    return result;
}

void send_response(int fd, struct http_request request) {
    char *response;
    char *file_content;
    int file_exists = access(request.request_path, F_OK);

    if (file_exists == -1) {
        file_content = read_complete_file("../404.html");
        response = response_as_string(file_content, "404", "NOT_FOUND");
    } else {
        file_content = read_complete_file(request.request_path);
        response = response_as_string(file_content, "200", "OK");
    }

    send(fd, response, strlen(response), 0);
    free(response);
    free(file_content);
}

int main() {
    char buffer[BUF_SIZE];
    struct addrinfo hints;
    struct addrinfo *result;

    socklen_t peer_addr_size;
    struct sockaddr_storage peer_addr;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int s = getaddrinfo("localhost", PORT, &hints, &result);
    if (s != 0) {
        printf("Failed to getaddrinfo");
        return -1;
    }

    // socket
    int sfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sfd == -1) {
        printf("Failed to create socket.");
        return -1;
    }
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0)
        return -1;

    // bind
    int bound = bind(sfd, result->ai_addr, result->ai_addrlen);
    if (bound == -1) {
        printf("Failed to bind.");
        return -1;
    }

    // listen
    int listened = listen(sfd, BACKLOG);
    if (listened == -1) {
        printf("Failed to listen.");
        return -1;
    }

    // add main socket to poll
    struct pollfd *pfds;
    pfds = calloc(MAX_CLIENTS, sizeof(struct pollfd));

    pfds[0].fd = sfd;
    pfds[0].events = POLL_IN;
    nfds_t nfds = 1;


    printf("Starting event loop.\n");

    for (;;) {
        int polled = poll(pfds, nfds, MAX_CLIENTS);
        if (polled == -1) {
            printf("Failed to poll.");
            return -1;
        }

        if (pfds[0].revents & POLL_IN) {
            printf("New incoming socket connection.\n");
            // accept
            int new_fd = accept(sfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
            if (new_fd != -1) {
                printf("Accepted new socket.\n");
                pfds[nfds].fd = new_fd;
                pfds[nfds].events = POLL_IN;
                nfds++;
            }
        }

        printf("Number of connected sockets: %i\n", nfds - 1);

        for (nfds_t i = 1; i < nfds; i++) {

            if (pfds[i].revents & POLL_IN) {
                printf("Data ready on socket [%i].\n", i);
                ssize_t data_size = recv(pfds[i].fd, buffer, BUF_SIZE, 0);

                if (data_size == -1) {
                    printf("Error on read.\n");
                    printf("Closing socket [%i].\n", i);
                    pfds[i] = pfds[--nfds];
                } else if (data_size == 0) {
                    printf("Closing socket [%i].\n", i);
                    pfds[i] = pfds[--nfds];
                } else {
                    printf("Message received. Mirroring back.\n");
                    struct http_request parsed_request = parse_http_request(buffer, data_size);
                    send_response(pfds[i].fd, parsed_request);
                }
            }
        }
    }
}
