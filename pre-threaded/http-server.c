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

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_not_empty = PTHREAD_COND_INITIALIZER;

int fd_buf[MAX_CLIENTS];
int buf_size = 0;

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
	(void)arg;
	pthread_t tid = pthread_self();

	while (1) {
		int fd;
		// condition variable; wait cond_not_empty
		//
		pthread_mutex_lock(&lock);
		while (buf_size == 0) {
			pthread_cond_wait(&cond_not_empty, &lock);		
		}
		
		buf_size--;
		fd = fd_buf[buf_size];

		pthread_cond_signal(&cond_not_full);
		pthread_mutex_unlock(&lock);

		printf("worker: %lu request picked up\n", (unsigned long)tid);
		handle_http_request(fd);
		printf("worker: %lu request handled successfully\n", (unsigned long)tid);
		shutdown(fd, SHUT_WR);
		if (close(fd) == -1) {
			perror("close");
		}
	}
}




int main() {
	pthread_t thread_ids[NUM_THREADS];
	signal(SIGPIPE, SIG_IGN);
	
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_create(&thread_ids[i], NULL, handle_request, NULL); 
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
