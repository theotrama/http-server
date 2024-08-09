#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <libc.h>
#include <sys/poll.h>

#define BUF_SIZE 50
#define BACKLOG 10
#define PORT "8080"
#define MAX_CLIENTS 1000

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
                    return -1;
                } else if (data_size == 0) {
                    printf("Closing socket [%i].\n", nfds);
                    pfds[i] = pfds[--nfds];
                }
                else {
                    printf("Message received. Mirroring back.\n");
                    send(pfds[i].fd, buffer, data_size, 0);
                }
            }
        }
    }
}