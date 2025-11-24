# Threads, sockets, and HTTP

This week we dove into threads. Last week, I explored processes and built a mini shell, so threads were the natural next
step. They let you introduce concurrency into your application without the heavy overhead of processes. Threads also
make data sharing simpler: unlike processes, which have separate stacks, heaps, and variables and communicate only via
IPC, threads share the same memory space within a process. Each thread belongs to a process, but there’s no hierarchy
between threads.

## Mutexes and condition variables

Shared memory comes with its own set of problems: race conditions, deadlocks, and other tricky concurrency issues.
Fortunately, the C standard library provides tools like mutexes and condition variables to handle these challenges
safely.

### Mutexes

Suppose two threads share the same variable between each other. If both read it, increment it, and write back to it
without coordination,
updates can override each other and produce invalid results. Luckily, there is a solution for this problem: mutexes.
Whenever we update a shared variable we first acquire a mutex. Thus, no other thread can access the variable during that
time.
Then we do our intended operation and unlock the variable again. This ensures that we only one thread can perform the
critical path
at any point in time.

```c 
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int counter = 0;

void *run_thread(void *arguments) {
    for (int i = 0; i < 100; i++) {
        pthread_mutex_lock(&lock);   // lock before accessing counter
        counter++;                    // increment safely
        pthread_mutex_unlock(&lock); // unlock after update
    }
}
```

### Condition variables

In the classic producer-consumer setup a producer generates items and a consumer processes them using a shared buffer.
A mutex can ensure mutually exclusive access on the buffer, but without condition variables the threads would be
endlessly
spinning whenever the buffer is empty or full.

- The producer can only write data when the buffer is not full. It spins endlessly checking if the buffer is not full.
- The consumer can only consume data when the buffer is not empty. It spins endless checking if the buffer is not empty.

Condition variables solve this by letting a thread sleep until a condition is true. The consumer sleeps till the buffer
is not empty.
The producer sleeps until the buffer is not full. For letting a thread on a condition we use `pthread_cond_wait` and to
wake
another thread up from a condition we use `pthread_cond_signal`. We will explore a real scenario of condition variables
in our HTTP server later on.

```c
#define MAX_BUF 10

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_full  = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;

int buf[MAX_BUF];
int n = 0;

/* Consumer */
void *consume(void *arg) {
    pthread_mutex_lock(&lock);
    while (n == 0)
        pthread_cond_wait(&not_empty, &lock);

    int item = buf[--n];
    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&lock);
    return NULL;
}

/* Producer */
void produce(int item) {
    pthread_mutex_lock(&lock);
    while (n == MAX_BUF)
        pthread_cond_wait(&not_full, &lock);

    buf[n++] = item;
    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&lock);
}
```

This pattern eliminates busy loops and ensures both producer and consumer sleep efficiently until the buffer state
allows progress.

## Serving millions of clients concurrently

In the early days servers were slow and could only handle a handful of clients at once. With the rise of the internet
the demand exploded, and suddenly handling thousands of concurrent connections became a real problem.
The obvious idea is: just spawn a thread per connection. But threads are heavy, expensive to create, and context
switching destroys performance. So we need something smarter.

The first improvement is a thread pool. Instead of creating a thread for every request, we create a fixed set of threads
at startup and reuse them. This already avoids the worst overhead, but it still doesn’t scale massively. Threads remain
expensive, and large numbers of them still slow the system down.

Then nginx came along and changed the game by going fully event-loop based, built around the `poll`/`epoll` system
calls.
Instead of one thread per connection, you have one thread running an event loop that waits for events on many file
descriptors at once. Igor Sysoev used this model to solve the famous C10K problem—serving ten thousand clients on a
single machine.

A single-threaded event loop is already fast, but you can go even further by combining both worlds: multithreading and
polling. Imagine ten threads, each with its own poll loop and its own list of file descriptors. Each thread blocks until
one of its descriptors becomes readable, handles the work, and goes back to polling. With this hybrid approach you can
scale far beyond the old limits.

Modern runtimes follow this pattern. Rust with Tokio uses worker threads + event loops. Go hides everything behind
extremely lightweight goroutines. And Java eventually caught up with virtual threads. The goal is always the same: serve
massive numbers of clients without paying the cost of one OS thread per connection.

## Let's write our own HTTP server

This week we explore threads by implementing a prethreaded HTTP server with a clear project structure: main server,
router, request parsers, and handlers. We'll focus purely on the socket handling. The server works as follows:

- One worker thread maintaining the server socket and accepts incoming connections.
- The worker thread puts each incoming client connection into a buffer as long as there is space in the buffer
  available.
- Multiple worker threads that race to consume the incoming client connections, i.e. obtaining the client socket from
  the buffer.
- Condition variables to ensure that producer and consumer are not spinning in a busy loop when the buffer is full or
  empty.

### Producer loop

The main thread (producer) accepts incoming client connections. For each connection, it acquires the lock and checks
if the buffer is full. If the buffer is full, it waits until space becomes available. This follows the same pattern as
in the condition variable section.

```c 
int fd_buf[MAX_CLIENTS];

int main() {
	pthread_t thread_ids[NUM_THREADS];
	signal(SIGPIPE, SIG_IGN);

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
}
```

### Consumer loop

Multiple consumer threads race to take file descriptors from the buffer. Each thread locks the buffer, waits if it is
empty, pops a file descriptor, signals that there is space, unlocks, and then handles the HTTP request before closing
the connection.

```c 
void *handle_request(void *arg) {
	(void)arg;
	pthread_t tid = pthread_self();

	while (1) {
		pthread_mutex_lock(&lock);
		while (buf_size == 0) {
			pthread_cond_wait(&cond_not_empty, &lock);		
		}
		
		buf_size--;
		fd = fd_buf[buf_size];

		pthread_cond_signal(&cond_not_full);
		pthread_mutex_unlock(&lock);

		handle_http_request(fd);
		if (close(fd) == -1) {
			perror("close");
		}
	}
}
```

## Let's go for more speed

Prethreading with a fixed set of worker threads speeds up the server compared to creating a new request per thread.
We can go further by combining the thread pool with an event loop. In that scenario each thread runs its own event
pool and handles dozens of client connections concurrently. During each loop it calls the `poll` system call which
tells the worker which client sockets are ready to be read. The indication is stored in an array of `pollfd` structs.

```c 
struct pollfd {
    int   fd;
    short events;
    short revents;
};
```

After polling, we iterate the array and check `revents`. If it’s `POLLIN`, there’s data ready to read. If it’s `POLLHUP`
or `POLLERR`, the client closed the connection or there was an error, and we can safely close the socket and remove it:

```c
int polled = poll(clientpfds[id], nfds[id], POLL_TIMEOUT);
if (polled == -1) {
            perror("Failed to poll.");
            continue;
    }
for (int i = 0; i < nfds[id]; i++) {
    if (clientpfds[id][i].revents & POLLIN) {
        handle_http_request(clientpfds[id][i].fd); // Handle the HTTP request
    }
    else if (clientpfds[id][i].revents & (POLLHUP | POLLERR)) {
        pop_fd(clientpfds[id][i].fd, id); // Closes the socket and removes it from the array
    }
}
```

This setup can handle a huge number of connections efficiently. It’s not perfect—blocking operations in
`handle_http_request` still stall the event loop—but for our learning server, it’s enough.

## Wrapping up

This week we covered a lot. We started with threads, explored mutexes for safe access to shared data, and saw how
condition variables prevent busy loops. We then implemented a prethreaded HTTP server with a thread pool, and finally
scaled it further by combining each thread pool with its own event loop.

## Caveats

This server is not production-ready. I haven’t tested it with `valgrind` for memory leaks, and it could crash or be
exploited. But as a learning project, it’s ideal: it gives a deep understanding of HTTP, sockets, threads, and
concurrency. Coming from a Java background, it’s also illuminating to see how strings work under the hood in C.