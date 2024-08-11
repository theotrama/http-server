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

struct http_request {
    enum HttpMethod http_method;
    char *request_path;
    enum HttpVersion http_version;
};

struct http_response {
    enum HttpMethod http_method;
    char *message_body;
    enum HttpVersion http_version;

};

char *response_as_string(struct http_response response) {
    char *response_array = "";

    if (response.http_version == VERSION_1_1) {
        char *ptr = realloc(response_array, strlen(response_array) + strlen("HTTP/1.1"));
        strcat(ptr, "HTTP/1.1");
    }

    return "1";
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

void create_http_response(struct http_request request) {

    if (access(request.request_path, F_OK) != 0) {
        // file does not exist -> return 404
    }

    int fd = open(request.request_path, O_RDONLY, 0);
    if (fd == -1) {
        printf("Failed to open file.\n");
    }

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
                }
                else {
                    printf("Message received. Mirroring back.\n");
                    struct http_request parsed_request = parse_http_request(buffer, data_size);


                    char *response = "HTTP/1.1 200 OK \r\nContent-Length:1\r\n\r\n2";
                    send(pfds[i].fd, response, data_size, 0);
                }
            }
        }
    }

}
