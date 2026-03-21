# CXO - Minimalist Static Blog Engine Makefile
# Copyright (c) 2026 Aq!u
# MIT License

# Platform detection
UNAME_S := $(shell uname -s)

# Compiler settings
ifeq ($(UNAME_S),Darwin)
    # macOS - use clang
    CC = clang
    INCLUDES = -I. -I./include
    LIBDIRS =
else
    # Linux and others - use gcc
    CC = gcc
    INCLUDES = -I. -I./include
    LIBDIRS =
endif

CFLAGS = -Wall -Wextra -std=c11 -O2
LDFLAGS =

# Libraries (empty - all dependencies embedded)
LIBS =

# Source files
CXO_SRCS = src/main.c src/config.c src/renderer.c src/linker.c \
           src/parser.c src/scanner.c src/context.c src/arena.c src/toml.c

# cmark sources (embedded, exclude main.c)
CMARK_SRCS = $(filter-out src/cmark/main.c, $(wildcard src/cmark/*.c))

# All sources
SRCS = $(CXO_SRCS) $(CMARK_SRCS)

# Object files
OBJS = $(SRCS:.c=.o)

# Targets
TARGET = cxo
TEST_DIR = tests
TEST_TARGETS = $(TEST_DIR)/test_scanner $(TEST_DIR)/test_parser \
               $(TEST_DIR)/test_linker $(TEST_DIR)/test_config \
               $(TEST_DIR)/test_renderer

# cmark objects for tests
CMARK_OBJS = $(filter-out src/cmark/main.o, $(CMARK_SRCS:.c=.o))

# Default target
all: $(TARGET)

# Build main executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Test binaries
$(TEST_DIR)/test_scanner: $(TEST_DIR)/test_scanner.c src/scanner.c src/context.c src/arena.c
	$(CC) $(CFLAGS) -I. -o $@ $^

$(TEST_DIR)/test_parser: $(TEST_DIR)/test_parser.c src/parser.c src/scanner.c \
                         src/context.c src/arena.c $(CMARK_OBJS)
	$(CC) $(CFLAGS) -I. $(INCLUDES) -o $@ $^

$(TEST_DIR)/test_linker: $(TEST_DIR)/test_linker.c src/linker.c src/parser.c src/scanner.c \
                         src/context.c src/arena.c $(CMARK_OBJS)
	$(CC) $(CFLAGS) -I. $(INCLUDES) -o $@ $^

$(TEST_DIR)/test_config: $(TEST_DIR)/test_config.c src/config.c src/context.c \
                         src/arena.c src/toml.c
	$(CC) $(CFLAGS) -I. $(INCLUDES) -o $@ $^

$(TEST_DIR)/test_renderer: $(TEST_DIR)/test_renderer.c src/renderer.c src/linker.c \
                           src/parser.c src/scanner.c src/context.c src/arena.c $(CMARK_OBJS)
	$(CC) $(CFLAGS) -I. $(INCLUDES) -o $@ $^

# Run all tests
test: $(TEST_TARGETS)
	@echo "Running tests..."
	@./$(TEST_DIR)/test_scanner && echo "scanner: PASS" || echo "scanner: FAIL"
	@./$(TEST_DIR)/test_parser && echo "parser: PASS" || echo "parser: FAIL"
	@./$(TEST_DIR)/test_linker && echo "linker: PASS" || echo "linker: FAIL"
	@./$(TEST_DIR)/test_config && echo "config: PASS" || echo "config: FAIL"
	@./$(TEST_DIR)/test_renderer && echo "renderer: PASS" || echo "renderer: FAIL"

# Clean build artifacts (only binaries, not source files)
clean:
	rm -f $(OBJS) $(TARGET)
	rm -f $(TEST_DIR)/test_scanner $(TEST_DIR)/test_parser $(TEST_DIR)/test_linker
	rm -f $(TEST_DIR)/test_config $(TEST_DIR)/test_renderer
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
