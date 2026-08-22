# CXO - Minimalist Static Blog Engine Makefile
# Copyright (c) 2026 Aq!u
# MIT License

# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)

# Compiler settings
ifeq ($(UNAME_S),Darwin)
    # macOS - use clang
    CC = clang
else
    # Linux, MinGW, MSYS2 - use gcc
    CC = gcc
endif

CPPFLAGS ?=
CPPFLAGS += -I. -I./include
CFLAGS ?= -Wall -Wextra -std=c11 -O2
LDFLAGS ?=

# Installation prefix
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# Libraries (empty - all dependencies embedded)
LIBS ?=

# Platform-specific commands
ifeq ($(OS),Windows_NT)
    EXEEXT = .exe
    RM = cmd /c del /f /q 2>nul
    RMDIR = cmd /c rmdir /s /q 2>nul
    LIBS += -lws2_32
else
    EXEEXT =
    RM = rm -f
    RMDIR = rm -rf
endif

# Source files
CXO_SRCS = src/main.c src/cmd_build.c src/cmd_init.c src/cmd_serve.c src/cmd_deploy.c src/config.c src/renderer.c \
           src/path_util.c src/template.c src/render_posts.c src/render_index.c \
           src/render_taxonomy.c src/render_feeds.c \
           src/linker.c src/parser.c src/scanner.c src/context.c src/arena.c src/toml.c src/lang.c

# cmark sources (embedded, exclude main.c)
CMARK_SRCS = $(filter-out src/cmark/main.c, $(wildcard src/cmark/*.c))

# All sources
SRCS = $(CXO_SRCS) $(CMARK_SRCS)

# Object files
OBJS = $(SRCS:.c=.o)

# Dependency files (for header tracking)
DEPS = $(SRCS:.c=.d)

# Targets
TARGET = cxo$(EXEEXT)
TEST_DIR = tests
TEST_TARGETS = $(TEST_DIR)/test_scanner$(EXEEXT) \
               $(TEST_DIR)/test_parser$(EXEEXT) \
               $(TEST_DIR)/test_linker$(EXEEXT) \
               $(TEST_DIR)/test_config$(EXEEXT) \
               $(TEST_DIR)/test_renderer$(EXEEXT) \
               $(TEST_DIR)/test_fixture$(EXEEXT)

# cmark objects for tests
CMARK_OBJS = $(filter-out src/cmark/main.o, $(CMARK_SRCS:.c=.o))

# Default target
all: $(TARGET)

# Build main executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)

# Compile source files with dependency generation
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

# Include dependency files
-include $(DEPS)

# Test binaries
$(TEST_DIR)/test_scanner$(EXEEXT): $(TEST_DIR)/test_scanner.c src/scanner.c src/context.c src/arena.c src/lang.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TEST_DIR)/test_parser$(EXEEXT): $(TEST_DIR)/test_parser.c src/parser.c src/scanner.c \
                         src/context.c src/arena.c src/lang.c $(CMARK_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TEST_DIR)/test_linker$(EXEEXT): $(TEST_DIR)/test_linker.c src/linker.c src/parser.c src/scanner.c \
                         src/context.c src/arena.c src/lang.c $(CMARK_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TEST_DIR)/test_config$(EXEEXT): $(TEST_DIR)/test_config.c src/config.c src/context.c \
                         src/arena.c src/toml.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TEST_DIR)/test_renderer$(EXEEXT): $(TEST_DIR)/test_renderer.c src/renderer.c src/path_util.c \
                           src/template.c src/render_posts.c src/render_index.c \
                           src/render_taxonomy.c src/render_feeds.c \
                           src/linker.c src/parser.c src/scanner.c src/context.c \
                           src/arena.c src/lang.c $(CMARK_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(TEST_DIR)/test_fixture$(EXEEXT): $(TEST_DIR)/test_fixture.c src/cmd_build.c src/config.c \
                          src/toml.c src/renderer.c src/path_util.c \
                          src/template.c src/render_posts.c src/render_index.c \
                          src/render_taxonomy.c src/render_feeds.c \
                          src/linker.c src/parser.c src/scanner.c src/context.c \
                          src/arena.c src/lang.c $(CMARK_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# Run all tests (stop on first failure)
test: $(TEST_TARGETS)
	@echo "Running tests..."
	@./$(TEST_DIR)/test_scanner$(EXEEXT) && echo "scanner: PASS" || (echo "scanner: FAIL" && exit 1)
	@./$(TEST_DIR)/test_parser$(EXEEXT) && echo "parser: PASS" || (echo "parser: FAIL" && exit 1)
	@./$(TEST_DIR)/test_linker$(EXEEXT) && echo "linker: PASS" || (echo "linker: FAIL" && exit 1)
	@./$(TEST_DIR)/test_config$(EXEEXT) && echo "config: PASS" || (echo "config: FAIL" && exit 1)
	@./$(TEST_DIR)/test_renderer$(EXEEXT) && echo "renderer: PASS" || (echo "renderer: FAIL" && exit 1)
	@./$(TEST_DIR)/test_fixture$(EXEEXT) && echo "fixture: PASS" || (echo "fixture: FAIL" && exit 1)

# Reproduce the clean build and test sequence used by CI.
ci:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) test

# Clean build artifacts (only build files, preserve public/)
clean:
	-$(RM) $(OBJS) $(DEPS) $(TARGET)
	-$(RM) $(TEST_TARGETS)
	-$(RM) src/cmark/*.o src/cmark/*.d

# Deep clean including generated site
distclean: clean
	-$(RMDIR) public/

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
	@scan-build $(CC) $(CPPFLAGS) $(CFLAGS) -c src/main.c -o /dev/null 2>/dev/null || echo "scan-build: not installed (run 'brew install llvm')"

# Build and serve locally for development
dev: $(TARGET)
	@./$(TARGET) build && ./$(TARGET) serve

# Phony targets
.PHONY: all test ci clean distclean install uninstall check dev
