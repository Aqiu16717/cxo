# CXO - AI Agent Guide

## Project Overview

**CXO** is a minimalist, high-performance static blog engine written in pure C (C11).

- **Project Status**: Production ready - all core modules implemented
- **Primary Goal**: Native bilingual (Chinese/English) support, zero build dependencies, extreme compilation speed
- **License**: MIT License (Copyright 2026 Aq!u)
- **Language**: Project documentation is primarily in Chinese

## Technology Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| **Language** | Pure C (C11) | Maximum performance, no runtime dependencies, low-level control |
| **Markdown Parser** | libcmark (embedded) | Industry standard, pure C implementation, extremely fast |
| **Config Parser** | toml-c (embedded) | TOML standard compatible, minimal code footprint |
| **Memory Management** | Arena Allocation | Unified alloc/free for static build scenarios, eliminates fragmentation |
| **Build Tool** | GNU Make | Simple, reliable, cross-platform |

### Embedded Dependencies

All third-party dependencies are embedded in the source tree (zero external dependencies):

- `src/cmark/` - libcmark markdown parser (excludes main.c)
- `src/toml.c` + `include/toml.h` - toml-c configuration parser
- `include/arena.h` + `src/arena.c` - Single-header arena allocator library

## Directory Structure

```
cxo/
├── LICENSE              # MIT License
├── README.md            # Brief project description
├── AGENTS.md            # This file
├── design.md            # Detailed Chinese design document
├── CODING_STYLE.md      # Coding style guidelines (Chinese)
├── Makefile             # Build system
├── .gitignore           # Git ignore rules for C projects
├── config.toml          # Site configuration file
├── src/                 # C source files
│   ├── main.c           # Main entry point with CLI commands
│   ├── cmd_init.c       # Project initialization commands (init/new/clean)
│   ├── scanner.c        # Content directory scanner
│   ├── parser.c         # Markdown and frontmatter parser
│   ├── context.c        # Context and entry management
│   ├── linker.c         # Entry linker for bilingual support
│   ├── renderer.c       # HTML template renderer
│   ├── config.c         # TOML configuration parser
│   ├── arena.c          # Arena allocator implementation wrapper
│   ├── toml.c           # TOML parser (embedded)
│   └── cmark/           # libcmark markdown parser (embedded)
├── include/             # Header files
│   ├── cxo.h            # Core data structures and API
│   ├── cxo_error.h      # Error codes
│   ├── arena.h          # Single-header arena allocator library
│   ├── toml.h           # TOML parser header
│   └── cmark*.h         # libcmark headers
├── tests/               # Test programs
│   ├── test_scanner.c   # Scanner module test
│   ├── test_parser.c    # Parser module test
│   ├── test_linker.c    # Linker module test
│   ├── test_config.c    # Config parser test
│   └── test_renderer.c  # Renderer test
├── content/             # Content directory
│   ├── zh/              # Chinese articles (.md)
│   │   └── hello.md
│   └── en/              # English articles (.md)
│       └── hello.md
└── themes/              # HTML templates
    └── default/         # Default theme
        ├── style.css
        └── post.html
```

## Core Architecture

### Build Pipeline

1. **Init**: Load `config.toml`, initialize global context and memory pool (Arena)
2. **Scanner**: Recursively traverse `content/zh` and `content/en`, build file manifest
3. **Parser**: Extract Markdown front-matter (YAML format with `---` delimiters), call libcmark to convert body to HTML fragments
4. **Linker**: Use hash table to link Chinese/English `cxo_entry_t` structs by `id` field
5. **Renderer**: Inject data into HTML templates, output to `public/` directory

### Core Data Structures

Defined in `include/cxo.h`:

```c
/* Single blog entry */
typedef struct cxo_entry {
    char* id;               /* Cross-language unique identifier */
    char* lang;             /* "zh" or "en" */
    char* title;            /* Article title */
    char* date;             /* Publication date */
    char* slug;             /* URL path name */
    char* html_content;     /* Parsed HTML content */
    char* md_content;       /* Raw markdown content (file path) */
    struct cxo_entry* peer; /* Pointer to translation in other language */
} cxo_entry_t;

/* Site-wide context */
typedef struct {
    cxo_entry_t** entries;  /* Dynamic array storing all articles */
    size_t count;
    size_t capacity;
    char* base_url;
    char* theme_path;
    char* site_title;
    char* site_description;
} cxo_context_t;
```

### I18n Routing

Bilingual support is hardcoded in generation logic:

- **Chinese (default)**: `public/posts/my-tech-blog.html`
- **English**: `public/en/posts/my-tech-blog.html`

During rendering, check `peer` pointer. If present, populate `{{nav_lang_switch}}` template variable with corresponding URL for one-click language switching.

## Build Commands

### Build the Project

```bash
# Build the main cxo executable
make

# Build and run all tests
make test

# Clean build artifacts
make clean

# Static analysis with cppcheck and scan-build
make check

# Install to system (optional)
make install

# Uninstall from system
make uninstall
```

### CLI Commands

```bash
# Initialize a new CXO project
cxo init [dir]

# Create a new blog post
cxo new "Article Title"

# Build the static site
cxo build    # or: cxo g

# Start development server
cxo serve [port]  # default port: 8080

# Clean build output
cxo clean

# Show version
cxo version  # or: cxo -v

# Show help
cxo help     # or: cxo -h
```

### Development Server

The built-in development server (`cxo serve`) provides:

- **Static file serving**: Serves files from `public/` directory
- **MIME type detection**: Auto-detects content types for common file extensions
- **Path traversal protection**: Rejects requests containing `..` sequences
- **Graceful shutdown**: Press Ctrl+C to stop

```bash
# Start server on default port 8080
cxo serve

# Start server on custom port
cxo serve 3000

# Server output:
# CXO development server running at http://localhost:8080
# Serving directory: public/
# Press Ctrl+C to stop
```

### Manual Compilation (without Makefile)

```bash
# Build main executable manually
gcc -Wall -Wextra -std=c11 -O2 -I. -I./include \
    src/main.c src/cmd_init.c src/config.c src/renderer.c src/linker.c \
    src/parser.c src/scanner.c src/context.c src/arena.c src/toml.c \
    src/cmark/*.c -o cxo
```

## Testing Instructions

### Running Tests

```bash
# Build and run all tests
make test

# Individual test binaries (built by make)
./tests/test_scanner    # Tests content directory scanning
./tests/test_parser     # Tests markdown and frontmatter parsing
./tests/test_linker     # Tests bilingual entry linking
./tests/test_config     # Tests TOML configuration parsing
./tests/test_renderer   # Tests HTML site generation
```

### Test Output Format

Tests output PASS/FAIL for each operation. Successful run shows:
- Arena creation
- Context creation
- Module operation (scan/parse/link/render)
- Cleanup
- Final success message

### Expected Test Results

All tests expect:
- `config.toml` exists in project root
- `content/zh/hello.md` and `content/en/hello.md` exist
- Both files have matching `id: hello` in frontmatter

## Content Format

Markdown files should include YAML frontmatter:

```markdown
---
id: hello
title: Article Title
date: 2026-03-19
---

Content body in Markdown format...
```

The `id` field is used to associate Chinese and English versions of the same article.

## Configuration File

`config.toml` format:

```toml
[site]
title = "My Blog"
description = "A minimalist blog powered by CXO"
base_url = "https://example.com"

[theme]
path = "themes/default"
```

Configuration is optional - defaults are used if file not found.

## Code Style Guidelines

Project follows **Linux Kernel Style** with specific rules:

### Naming Conventions

| Type | Style | Example |
|------|-------|---------|
| Types | snake_case_t | `arena_t`, `cxo_entry_t` |
| Functions | snake_case | `arena_create()`, `cxo_scan_content()` |
| Macros | UPPER_CASE | `CXO_VERSION`, `ARENA_ALIGNMENT` |
| Variables | snake_case | `chunk_size`, `entry_count` |

### Pointer Style

```c
/* Correct */
arena_t* a;
void* ptr;
char* str;

/* Incorrect */
arena_t *a;
void * ptr;
```

### Braces and Control Statements

- **All** control statements must use braces, even single-line
- Code blocks on new lines

```c
/* Correct */
if (!a) {
    return NULL;
}

while (r) {
    r = r->next;
}

/* Incorrect */
if (!a) return NULL;
if (!a)
    return NULL;
```

### Comments

- Comments must be on separate lines, not inline with code

```c
/* Correct */
/* Total size of this region */
size_t capacity;

/* Incorrect */
size_t capacity; /* Total size */
```

### Formatting

- **Indentation**: 2 spaces (no tabs)
- **Line length**: Maximum 80 characters

### Memory Management

- **Mandatory**: Use arena allocator for all allocations
- **Prohibited**: Do not use malloc/free in core logic
- Arena is created at build start and destroyed at end (atomic cleanup)

### Function Design (Small & Atomic)

- **Ideal length**: Under 50 lines
- **Maximum nesting**: 3 levels
- **Single responsibility**: Each function solves one problem

## Git Commit Guidelines

### Commit Message Format

- Keep it concise, **one line is sufficient**
- Use imperative mood, describe what was done
- Lowercase first letter

```
add arena.h: single-header memory allocator library
update test suite for new API style
fix pointer style to Linux kernel style
```

### Commit Granularity

- **Do not commit too much at once**
- Each commit should do one thing
- Keep commit history clean and readable

### Good Examples

```bash
# Good - separate commits
git add arena.h
git commit -m "add arena memory allocator library"

git add test_arena.c
git commit -m "add comprehensive test suite"

# Bad - commit everything at once
git add .
git commit -m "add all files"
```

## Development Workflow

1. **Branch Management**: All new features must be developed in feature branches, not directly on main
2. **Code Review**: Changes should be reviewed before merging to main
3. **Testing**: Include tests for new functionality
4. **Incremental Development**: Follow "develop-test-iterate" cycle

## Key Design Principles

1. **Keep it simple, stupid** - Avoid over-engineering
2. **High performance** - Memory efficiency, minimal I/O, arena allocation
3. **Zero runtime dependencies** - All deps embedded, static binary preferred
4. **Bilingual first** - i18n is core feature, not an afterthought
5. **Memory safety** - Use arena allocator exclusively, avoid malloc/free in core logic
6. **Small & Atomic functions** - Ideal length under 50 lines, max 3-level nesting

## Error Codes

Defined in `include/cxo_error.h`:

| Code | Value | Description |
|------|-------|-------------|
| CXO_OK | 0 | Success |
| CXO_ERR_NOMEM | -1 | Out of memory |
| CXO_ERR_IO | -2 | I/O error |
| CXO_ERR_INVAL | -3 | Invalid argument |
| CXO_ERR_SCAN | -10 | Scan failed |
| CXO_ERR_NODIR | -11 | Directory not found |
| CXO_ERR_TOOMANY | -12 | Too many entries |
| CXO_ERR_PARSE | -20 | Parse failed |
| CXO_ERR_NOFILE | -21 | File not found |
| CXO_ERR_FMT | -22 | Invalid format |
| CXO_ERR_LINK | -30 | Link failed |
| CXO_ERR_DUPID | -31 | Duplicate entry id |
| CXO_ERR_RENDER | -40 | Render failed |
| CXO_ERR_NOTHEME | -41 | Theme not found |

Use `CXO_IS_ERR(rc)` macro to check if return value is an error.

## Implementation Status

### Completed Components

| Component | File | Status |
|-----------|------|--------|
| Arena Allocator | `include/arena.h`, `src/arena.c` | Complete |
| Core Data Structures | `include/cxo.h` | Complete |
| Error Codes | `include/cxo_error.h` | Complete |
| Context Management | `src/context.c` | Complete |
| Content Scanner | `src/scanner.c` | Complete |
| Markdown Parser | `src/parser.c` | Complete |
| Entry Linker | `src/linker.c` | Complete |
| Config Parser | `src/config.c` | Complete |
| Site Renderer | `src/renderer.c` | Complete |
| CLI Commands | `src/cmd_init.c` | Complete |
| Dev Server | `src/cmd_serve.c` | Complete |
| CLI Entry Point | `src/main.c` | Complete |
| Build System | `Makefile` | Complete |

### All Modules Ready

The project has reached feature-complete status for core static site generation functionality.

## References

- `design.md` - Detailed Chinese design document with architecture details
- `CODING_STYLE.md` - Complete coding style guide (Chinese)
- `LICENSE` - MIT License
- `README.md` - Brief project description
