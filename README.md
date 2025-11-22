# Threads, sockets, and HTTP

This week's learning session was focussed on threads. Last week I was working on understanding processes and built my own mini shell.
Naturally, threads were the next step. They offer a nice way to introduce concurrency into your application without the giant overhead as processes.
They also make data sharing much simpler as threads are not as strictly encapsulated as processes where each process
has its own stack, variables, and heap and the only way to communicate is via interprocess communication (IPC). Threads are always
attached to one process and there is no hierarchy between threads.




## Mutexes and condition variables
As it's easy for threads to share variables we open the pandora box for a whole plethora of problems like race conditions, mutexes,
deadlocks, and so on. Luckily the C standard library offers tools to deal with those problems

### Mutexes
Suppose we have two threads and they share a variable counter between each other. Now what happens if thread 1 reads the variable
then thread 2 reads the variable. They both increment the counter and write it back. But as thread 2 read the variable before thread 1 wrote back to it
thread 2 now basically overwrites the value. Thus, instead of incrementing by two in the end the counter is incorrectly incremented only
by 1. Luckily, there is a solution for this problem: mutexes. While in Java we have the `synchronized` key word the ensure
thread safe variable access in C we can make use of mutexes from the pthreads api. Basically, whenever we reach a critical path in our program
we need to first lock the mutex. Then we do our intended operation and unlock the variable again. This ensures that no other thread
can read or write to that variable until the mutex is unlocked. Let's look at some code.

```c 
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 10

int counter = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *run_thread(void *arguments) {
    for (int i = 0; i < 100; i++) {
        pthread_mutex_lock(&lock);   // lock before accessing counter
        counter++;                    // increment safely
        pthread_mutex_unlock(&lock); // unlock after update
    }

	return NULL;
}

int main() {
    pthread_t pthreads[NUM_THREADS];
    int thread_ids[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i;
        pthread_create(&pthreads[i], NULL, run_thread, &thread_ids[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(pthreads[i], NULL);
    }

    printf("counter: %d\n", counter);
    return 0;
}
```
Here we can see that before we increment the counter we lock and afterwards we release the lock. Thus, our update is now thread safe.


### Condition variables
A common problem in computer science is the producer-consumer problem. In this scenario we have a producer that creates data
and a consumer that needs to perform work on this data. In order to decouple the producer and the consumer we introduce a buffer
that is shared between the producer and the consumer. Whenever the producer creates data it acquires a lock on the buffer, writes the data into it 
and releases the lock again. On the other side the consumer checks the buffer for incoming data and as soon as data is on the buffer
it acquires the lock removes data from the buffer, processes the data and then releases the lock. Now there are two cases
where either the producer or the consumer could end up in a busy loop. If the buffer is full the, the producer would try to 
put data into it every iteration. But as there is no more space on the buffer it cannot. So it's endlessly spinning insides
it's while loop, always waiting for the buffer to have space. On the other side the same is true for the consumer. Whenever the buffer is empty
it would endlessly wait for data to finally become available. This would was precious CPU resources, but luckily there is also a solution
to this problem: condition variables and signals. Please be aware that this is only pseudo code in C syntax. Let's say we are listening
on a socket to listen for incoming client requests. In a multithreaded setup we could spawn a thread pool with multiple consumers, and
then have a single producer thread that waits for **new** incoming connections. Whenever a new connection is coming in 
the producer would put the file descriptor of that socket onto a buffer shared with the consumers. As soon as the socket
is put onto the buffer the consumers race for obtaining the socket from the buffer. The first one wins, acquires a lock and then processes
the request. Now below you can see what happens if the buffer is full. The producer (main loop) has a while loop that checks
whether the buf_size == MAX_CLIENTS. However, instead of endlessly spinning in this while loop it waits for a certain condition
called `pthread_cond_wait(&cond_not_full, &lock)`. This means, that it is waiting for the buffer to be not full. But how will the producer 
know about the buffer being not full once it is filled up completely? Well, that is the job of the consumer. Whenever, the consumer
pops a file descriptor from the buffer it signals `pthread_cond_signal(&cond_not_full)` thus waking up the producer from its sleep.
The same is true the other way around. The consumers are waiting until the buffer is not empty `pthread_cond_wait(&cond_not_empty, &lock)`.
Once the producer signals that the buffer is not empty the consumers are picking up their work again. This is a very elegant
way to avoid busy loops and massively decreases CPU consumption.


```c
define BACKLOG 10
#define MAX_CLIENTS 1024
#define NUM_THREADS 10
#define READ_BUF 1024

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_not_empty = PTHREAD_COND_INITIALIZER;

int fd_buf[MAX_CLIENTS];
int buf_size = 0;

void *handle_request(void *arg) {
	(void)arg;
	pthread_t tid = pthread_self();

	while (1) {
		int fd;
		// condition variable; wait cond_not_empty
		pthread_mutex_lock(&lock);
		while (buf_size == 0) {
			pthread_cond_wait(&cond_not_empty, &lock);		
		}
		
		buf_size--;
		fd = fd_buf[buf_size];

		pthread_cond_signal(&cond_not_full);
		pthread_mutex_unlock(&lock);

		printf("worker: %lu request picked up\n", (unsigned long)tid);
		handle_request(fd);
		printf("worker: %lu request handled successfully\n", (unsigned long)tid);
	}
}

int main() {
	pthread_t thread_ids[NUM_THREADS];
	
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_create(&thread_ids[i], NULL, handle_request, NULL); 
	}

	while (1) {
		int client_fd = accept(fd, (struct sockaddr *)(&client_addr), &socklen);

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
```

## Serving thousands of clients
Back in the old days servers were slow. They could server only a couple of clients concurrently. With the widespread adoption
of the internet the demand for handling multiple connections at the same time grew dramatically. The most simple solution would
be to just throw threads at that task. But threads are heavy and have a large overhead. Especially creating threads for every incoming
connection is costly. The first optimization is to use a thread pool utilizing the producer-consumer problem. In that scenario (as in the code above)
a fixed amount of threads are created at program start. Thus, the name prethreaded. But also this does not scale massively as threads
are expensive and especially context switching between threads is costly. Along came nginx and switching entirely to an event-loop based
setup that utilizes the `poll` system call with an event loop. The founder of nginx Igor Sysoev used this design to solve the C10K problem. Serving
ten thousand clients on the same machine. If you use a single thread for handling the event loop this is already fast but you can 
achieve even greater results if you combine the two approaches multithreading and event loop. What if we have ten threads and each thread
has its own list of file descriptors that it constantly `polls`, reading data only when it becomes available? Well with approach
we can achieve even greater results and that is basically how for example Rust and Tokio work. In Go all this is nicely abstracted
away by goroutines which are lightweight green threads that scale easily to millions of connections. Java caught up later with virtual threads.


## Let's write our own HTTP server
A couple of years ago I attempted this feat for the first time using an event loop based setup. I still remember how hard
it was for me to wrap my head around pointers and dynamically allocating memory at that time. Now after a couple of weeks
of exclusively using C this feels now much more natural. As this week's learning goal was threads I decided to write a prethreaded
http server this time with a much better project structure and a more capable parser. I structured my project along the main http-server, a
router, request parsers and handlers. This setup feels pretty good and if I wasn't too lazy to write tests it would also be a good
setup to test each module in isolation.



## Caveats
Don't use this in production. I know the code is not perfect, I never tested it with `valgrind` to rule out memory leaks and 
I'm sure that it is very make this server crash and exploit it. But for a learning project it is an ideal scope and gives you
a deep understanding of the HTTP protocol. Also string manipulation in C feels hard for me coming mainly Java background. But still
it's nice to see how strings are working under the hood.