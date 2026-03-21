# CXO - Minimalist Static Blog Engine Makefile
# Copyright (c) 2026 Aq!u
# MIT License

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
LDFLAGS =

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS
    INCLUDES = -I. -I/opt/homebrew/include
    LIBDIRS = -L/opt/homebrew/lib
endif
ifeq ($(UNAME_S),Linux)
    # Linux
    INCLUDES = -I.
    LIBDIRS =
endif

# Libraries
LIBS = -lcmark -linih

# Source files
SRCS = src/main.c src/config.c src/renderer.c src/linker.c \
       src/parser.c src/scanner.c src/context.c src/arena.c

# Object files
OBJS = $(SRCS:.c=.o)

# Targets
TARGET = cxo
TEST_TARGETS = test_scanner test_parser test_linker test_config test_renderer

# Default target
all: $(TARGET)

# Build main executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LIBDIRS) $(LIBS) $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Test binaries
test_scanner: test_scanner.c src/scanner.c src/context.c src/arena.c
	$(CC) $(CFLAGS) -I. -o $@ $^

test_parser: test_parser.c src/parser.c src/scanner.c src/context.c src/arena.c
	$(CC) $(CFLAGS) -I. $(INCLUDES) -o $@ $^ $(LIBDIRS) $(LIBS)

test_linker: test_linker.c src/linker.c src/parser.c src/scanner.c \
             src/context.c src/arena.c
	$(CC) $(CFLAGS) -I. $(INCLUDES) -o $@ $^ $(LIBDIRS) $(LIBS)

test_config: test_config.c src/config.c src/context.c src/arena.c
	$(CC) $(CFLAGS) -I. $(INCLUDES) -o $@ $^ $(LIBDIRS) -linih

test_renderer: test_renderer.c src/renderer.c src/linker.c src/parser.c \
               src/scanner.c src/context.c src/arena.c
	$(CC) $(CFLAGS) -I. $(INCLUDES) -o $@ $^ $(LIBDIRS) $(LIBS)

# Run all tests
test: $(TEST_TARGETS)
	@echo "Running tests..."
	@./test_scanner && echo "scanner: PASS" || echo "scanner: FAIL"
	@./test_parser && echo "parser: PASS" || echo "parser: FAIL"
	@./test_linker && echo "linker: PASS" || echo "linker: FAIL"
	@./test_config && echo "config: PASS" || echo "config: FAIL"
	@./test_renderer && echo "renderer: PASS" || echo "renderer: FAIL"

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET) $(TEST_TARGETS)
	rm -rf public/

# Install (optional)
install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/

# Uninstall
uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)

# Phony targets
.PHONY: all test clean install uninstall
