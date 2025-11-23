# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -Ihttp

# Shared HTTP sources
HTTP_SRCS = http/http-parser.c http/http-router.c http/http-handlers.c http/http-response.c
HTTP_OBJS = http-parser.o http-router.o http-handlers.o http-response.o

# Servers
SERVERS = prethreaded hybrid

# Executables
TARGETS = $(addsuffix /http-server.r,$(SERVERS))
ASAN_TARGETS = $(addsuffix /http-server.asan,$(SERVERS))

# Default target: build all servers
all: $(TARGETS)

# Build each server
prethreaded/http-server.r: prethreaded/http-server.c $(HTTP_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

hybrid/http-server.r: hybrid/http-server.c $(HTTP_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile shared HTTP sources to root .o files
http-parser.o: http/http-parser.c
	$(CC) $(CFLAGS) -c $< -o $@
http-router.o: http/http-router.c
	$(CC) $(CFLAGS) -c $< -o $@
http-handlers.o: http/http-handlers.c
	$(CC) $(CFLAGS) -c $< -o $@
http-response.o: http/http-response.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build AddressSanitizer-enabled servers
asan: CFLAGS += -fsanitize=address
asan: LDFLAGS += -fsanitize=address
asan: $(ASAN_TARGETS)

prethreaded/http-server.asan: prethreaded/http-server.c $(HTTP_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

hybrid/http-server.asan: hybrid/http-server.c $(HTTP_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Clean all objects and executables
clean:
	rm -f $(HTTP_OBJS)
	rm -f $(TARGETS) $(ASAN_TARGETS)

