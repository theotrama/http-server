#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#include "http-parser.h"
#include "http-router.h"

#define BACKLOG 10
#define MAX_CLIENTS 1024
#define NUM_THREADS 10
#define READ_BUF 1024
#define MAX_POLL_FDS 1024
#define POLL_TIMEOUT 50

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_not_empty = PTHREAD_COND_INITIALIZER;

int fd_buf[MAX_CLIENTS];
int buf_size = 0;
struct pollfd clientpfds[NUM_THREADS][MAX_POLL_FDS];
int nfds[NUM_THREADS];
int thread_indices[NUM_THREADS];


void add_fd(int fd, int tid) {

	if (nfds[tid] >= MAX_POLL_FDS) {
	    	close(new_fd);
		errno = EMFILE;
    		perror("Thread full, rejecting connection");
		return;
	}

	struct pollfd pfd = {
    		.fd = fd,
    		.events = POLLIN,
    		.revents = 0
	};
	clientpfds[i][nfds[tid]] = pfd;
	nfds[tid]++;
}

void pop_fd(int fd, int tid) {
	for (int i = 0; i < nfds[tid]; i++) {
		if (clientpfds[tid][i].fd == fd) {
			close(fd);
			clientpfds[tid][i] = clientpfds[tid][nfds[tid] - 1];
			nfds[tid]--;
			return;
		}
	}
}

int handle_http_request(int fd) {
	char buf[READ_BUF];
	int size = 0;
	int nbytes;
	char tmp[512];
	while ((nbytes = read(fd, tmp, sizeof(tmp))) > 0) {
		if ((size + nbytes) > READ_BUF - 1) { // leave space for null-termination
			errno = ENOMEM;
			return -1;
		}
		memcpy(buf + size, tmp, nbytes);
		size += nbytes;
		buf[size] = '\0';


		if (strstr(buf, "\r\n\r\n") != NULL) {
			break;
		}
	}

	http_request request;
	parse_http_request(buf, &request);
	
	http_response response;
	dispatch_request(&request, &response);

	write(fd, response.start_line, strlen(response.start_line));
	write(fd, "\r\n", 2);

	for (size_t i = 0; i < response.headers.count; i++) {
		
		write(fd, response.headers.headers[i].key, strlen(response.headers.headers[i].key));
		write(fd, ": ", 2);
		write(fd, response.headers.headers[i].value, strlen(response.headers.headers[i].value));
		write(fd, "\r\n", 2);	
	}
	write(fd, "\r\n", 2);
	write(fd, response.resp_body, strlen(response.resp_body));
	free_http_request(&request);
	free_http_response(&response);
	return 0;
}

void *handle_request(void *arg) {
	pthread_t tid = pthread_self();
	int id = *(int *)arg;
	while (1) {
		int fd;
		pthread_mutex_lock(&lock);
		if (buf_size != 0) {
			buf_size--;
			fd = fd_buf[buf_size];
			add_fd(fd, id);
		}
		pthread_mutex_unlock(&lock);

		
		int polled = poll(clientpfds[id], nfds[id], POLL_TIMEOUT);
		if (polled == -1) {
            		perror("Failed to poll.");
            		continue;
        	}
		for (int i = 0; i < nfds[id]; i++) {
			if (clientpfds[id][i].revents & POLL_IN) {
				printf("worker: %d request picked up\n", id);
				handle_http_request(clientpfds[id][i].fd);
				printf("worker: %d request handled successfully\n", id);
			}
			else if (clientpfds[id][i].revents & (POLLHUP | POLLERR)) {
				printf("worker: %d client disconnected. Clean up\n", id);
				pop_fd(clientpfds[id][i].fd, id);
				printf("worker: %d cleaned up successfully\n", id);
			}
		}

	}
}




int main() {
	pthread_t thread_ids[NUM_THREADS];
	signal(SIGPIPE, SIG_IGN);
	
	for (int i = 0; i < NUM_THREADS; i++) {
		thread_indices[i] = i;
		pthread_create(&thread_ids[i], NULL, handle_request, &thread_indices[i]); 
	}
	
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("socket");
		exit(1);
	}
	struct sockaddr_in address = {0};
	address.sin_family = AF_INET;
	address.sin_port = htons(8080);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	int opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
		perror("setsockopt SO_REUSEADDR");
		exit(1);
	}

 	if (bind(fd, (struct sockaddr *)(&address), sizeof(address)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(fd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	while (1) {
	
		struct sockaddr_in client_addr = {0};
		socklen_t socklen = sizeof(client_addr);
		int client_fd = accept(fd, (struct sockaddr *)(&client_addr), &socklen);
		if (client_fd == -1) {
			perror("accept");
			continue;
		}

		pthread_mutex_lock(&lock);
		while (buf_size == MAX_CLIENTS) {
			pthread_cond_wait(&cond_not_full, &lock);
		}

		fd_buf[buf_size] = client_fd;
		buf_size++;

		pthread_cond_signal(&cond_not_empty);
		pthread_mutex_unlock(&lock);
	}
	
	for (int i = 0; i < NUM_THREADS; i++) {
	    pthread_join(thread_ids[i], NULL);
	}
}
