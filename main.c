#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <libc.h>
#include <sys/poll.h>

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

char *response_as_string(struct http_response response) {
    char *http_version  = "HTTP/1.1";
    char *status_code = "200 OK";

    char content_length_header[100];
    sprintf(content_length_header, "Content-Length: %lu", response.content_length);

    char *result = malloc(strlen(http_version) + strlen(" ") + strlen(status_code) + strlen("\r\n") + strlen(content_length_header) + +strlen("\r\n\r\n") +
                                  response.content_length + 1);

    strcpy(result, http_version);
    strcat(result, " ");
    strcat(result, status_code);
    strcat(result, "\r\n");
    strcat(result, content_length_header);
    strcat(result, "\r\n\r\n");
    return result;
}

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

    char *http_version = strtok(NULL, "\r\n");
    enum HttpVersion httpVersion;
    if (strcmp(http_version, "HTTP/1.1") == 0) {
        httpVersion = VERSION_1_1;
    } else {
        httpVersion = UNDEFINED;
    }

    struct http_request request = {httpMethod, requested_path, httpVersion};

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

struct http_response create_http_response(struct http_request request) {

    struct http_response response;
    response.http_version = VERSION_1_1;
    response.httpStatus = OK;
    long file_size;

    char *directory_escape = "../";

    char *result = malloc(strlen(directory_escape) + strlen(request.request_path) + 1);
    strcpy(result, directory_escape);
    strcat(result, request.request_path);
    FILE *fd = fopen(result, "rb");
    if (fd == NULL) {
        printf("Error: Failed to open file '%s'.\n", request.request_path);
        response.httpStatus = NOT_FOUND;
        return response;
    }

    fseek(fd, 0L, SEEK_END);
    file_size = ftell(fd);
    rewind(fd);
    response.content_length = file_size;
    return response;
}

long get_file_size(char *file_name) {
    FILE *fd = fopen(file_name, "rb");
    if (fd == NULL) {
        printf("Error: Failed to open file '%s'.\n", file_name);
        return -1;
    }

    fseek(fd, 0L, SEEK_END);
    return ftell(fd);
}

void send_response(int fd, struct http_request request) {
    long file_size = get_file_size(request.request_path);
    if (file_size == -1) {

    }

    char *http_version  = "HTTP/1.1";
    char *status_code = "200 OK";
    char content_length_header[100];
    sprintf(content_length_header, "Content-Length: %lu", file_size);

    char *result = malloc(strlen(http_version) + strlen(" ") + strlen(status_code) + strlen("\r\n") + strlen(content_length_header) + +strlen("\r\n\r\n"));
    strcpy(result, http_version);
    strcat(result, " ");
    strcat(result, status_code);
    strcat(result, "\r\n");
    strcat(result, content_length_header);
    strcat(result, "\r\n\r\n");

    send(fd, result, strlen(result),0 );

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
                    struct http_response response = create_http_response(parsed_request);
                    send_response(pfds[i].fd, parsed_request);

                    char *response_as_str = response_as_string(response);

                }
            }
        }
    }

}
