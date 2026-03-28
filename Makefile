# CXO - Minimalist Static Blog Engine Makefile
# Copyright (c) 2026 Aq!u
# MIT License

# Platform detection
UNAME_S := $(shell uname -s)

# Compiler settings
ifeq ($(UNAME_S),Darwin)
    # macOS - use clang
    CC = clang
else
    # Linux and others - use gcc
    CC = gcc
endif

INCLUDES = -I. -I./include
CFLAGS = -Wall -Wextra -std=c11 -O2
LDFLAGS =

# Installation prefix
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# Libraries (empty - all dependencies embedded)
LIBS =

# Source files
CXO_SRCS = src/main.c src/cmd_init.c src/cmd_serve.c src/cmd_deploy.c src/config.c src/renderer.c \
           src/linker.c src/parser.c src/scanner.c src/context.c src/arena.c src/toml.c

# cmark sources (embedded, exclude main.c)
CMARK_SRCS = $(filter-out src/cmark/main.c, $(wildcard src/cmark/*.c))

# All sources
SRCS = $(CXO_SRCS) $(CMARK_SRCS)

# Object files
OBJS = $(SRCS:.c=.o)

# Dependency files (for header tracking)
DEPS = $(SRCS:.c=.d)

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

# Compile source files with dependency generation
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

# Include dependency files
-include $(DEPS)

# Test binaries
$(TEST_DIR)/test_scanner: $(TEST_DIR)/test_scanner.c src/scanner.c src/context.c src/arena.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(TEST_DIR)/test_parser: $(TEST_DIR)/test_parser.c src/parser.c src/scanner.c \
                         src/context.c src/arena.c $(CMARK_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(TEST_DIR)/test_linker: $(TEST_DIR)/test_linker.c src/linker.c src/parser.c src/scanner.c \
                         src/context.c src/arena.c $(CMARK_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(TEST_DIR)/test_config: $(TEST_DIR)/test_config.c src/config.c src/context.c \
                         src/arena.c src/toml.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(TEST_DIR)/test_renderer: $(TEST_DIR)/test_renderer.c src/renderer.c src/linker.c \
                           src/parser.c src/scanner.c src/context.c src/arena.c $(CMARK_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

# Run all tests (stop on first failure)
test: $(TEST_TARGETS)
	@echo "Running tests..."
	@./$(TEST_DIR)/test_scanner && echo "scanner: PASS" || (echo "scanner: FAIL" && exit 1)
	@./$(TEST_DIR)/test_parser && echo "parser: PASS" || (echo "parser: FAIL" && exit 1)
	@./$(TEST_DIR)/test_linker && echo "linker: PASS" || (echo "linker: FAIL" && exit 1)
	@./$(TEST_DIR)/test_config && echo "config: PASS" || (echo "config: FAIL" && exit 1)
	@./$(TEST_DIR)/test_renderer && echo "renderer: PASS" || (echo "renderer: FAIL" && exit 1)

# Clean build artifacts (only build files, preserve public/)
clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)
	rm -f $(TEST_DIR)/test_scanner $(TEST_DIR)/test_parser $(TEST_DIR)/test_linker
	rm -f $(TEST_DIR)/test_config $(TEST_DIR)/test_renderer
	rm -f src/cmark/*.o src/cmark/*.d

# Deep clean including generated site
distclean: clean
	rm -rf public/

# Install
install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/

# Uninstall
uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

# Static analysis with cppcheck
check:
	@echo "Running cppcheck..."
	@cppcheck --std=c11 --enable=all --suppress=missingIncludeSystem \
		--suppress=unusedFunction --suppress=toomanyconfigs \
		-I include src/ tests/ 2>/dev/null || echo "cppcheck: not installed (run 'brew install cppcheck' or 'apt-get install cppcheck')"
	@echo "Running scan-build..."
	@scan-build $(CC) $(CFLAGS) $(INCLUDES) -c src/main.c -o /dev/null 2>/dev/null || echo "scan-build: not installed (run 'brew install llvm')"

# Build and serve locally for development
dev: $(TARGET)
	@./$(TARGET) build && ./$(TARGET) serve

# Phony targets
.PHONY: all test clean distclean install uninstall check dev
