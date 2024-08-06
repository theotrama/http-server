#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <libc.h>

#define BUF_SIZE 50
#define BACKLOG 10
#define PORT "8081"

int main() {
    // socket
    // bind
    // listen
    // accept
    // read / write
    // close

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

    // accept
    for (;;) {
        peer_addr_size = sizeof peer_addr;
        int new_fd = accept(sfd, (struct sockaddr*) &peer_addr, &peer_addr_size);
        if (new_fd == -1) {
            printf("Failed to accept.");
            return -1;
        }

        // read and write
        ssize_t data_size = 1;
        while(data_size != 0) {
            printf("Reading data.\n");
            data_size = recv(new_fd, buffer, BUF_SIZE, 0);
            printf("data size: %zi\n", data_size);

            if (data_size == -1) {
                printf("Error on read.\n");
                return -1;
            } else {
                printf("Message received. Mirroring back.\n");
                send(new_fd, buffer, data_size, 0);
            }
            printf("\n");
        }
        printf("All data read. Closing connection.\n");
        close(new_fd);
    }
}