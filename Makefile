CC = gcc

INCLUDES = -I./include

CFLAGS = -Wall -Wextra -Wpedantic -std=c17 $(INCLUDES) -D_POSIX_C_SOURCE=200809L

# Debugging and sanitizers for tests
DEBUG_FLAGS = -O0 -g -fsanitize=address,undefined -DDEBUG

# Optimization for benchmarks
BENCH_FLAGS = -O3

.PHONY: all test bench clean

all: test

# Unit tests with AddressSanitizer and UndefinedBehaviorSanitizer
test: src/hashtable.c tests/test_hashtable.c
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $^ -o bin/test_runner
	./bin/test_runner

# Optimized benchmarks without sanitizers
bench: src/hashtable.c bench/bench_hashtable.c
	mkdir -p bin
	$(CC) $(CFLAGS) $(BENCH_FLAGS) $^ -o bin/bench_runner
	./bin/bench_runner

# Remove generated executables
clean:
	rm -rf bin

