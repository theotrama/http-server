# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -I../http

# Source files
SRCS = ../http/http-parser.c ../http/http-router.c ../http/http-handlers.c ../http/http-response.c async-server.c

# Object files in root
OBJS = http-parser.o http-router.o http-handlers.o http-response.o async-server.o

# Output executables
TARGET = async-server.r
ASAN_TARGET = async-server.asan

# Default target
all: $(TARGET)

# Build regular executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile HTTP sources to root .o files
http-parser.o: ../http/http-parser.c
	$(CC) $(CFLAGS) -c $< -o $@

http-router.o: ../http/http-router.c
	$(CC) $(CFLAGS) -c $< -o $@

http-handlers.o: ../http/http-handlers.c
	$(CC) $(CFLAGS) -c $< -o $@

http-response.o: ../http/http-response.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile server source
http-server.o: http-server.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build AddressSanitizer-enabled executable
asan: CFLAGS += -fsanitize=address
asan: LDFLAGS += -fsanitize=address
asan: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(ASAN_TARGET) $(OBJS)

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET) $(ASAN_TARGET)

