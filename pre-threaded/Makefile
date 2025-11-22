# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

# Source files
SRCS = http-parser.c http-server.c http-router.c http-handlers.c http-response.c

# Object files
OBJS = $(SRCS:.c=.o)

# Output executable
TARGET = http-server.r
ASAN_TARGET = http-server.asan

# Default target
all: $(TARGET)

# Build regular executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile .c into .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build AddressSanitizer-enabled executable
asan: CFLAGS += -fsanitize=address
asan: LDFLAGS += -fsanitize=address
asan: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(ASAN_TARGET) $(OBJS)

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET) $(ASAN_TARGET)

