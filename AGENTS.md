# CXO - AI Agent Guide

**CXO** is a minimalist, high-performance static blog engine written in pure C (C11).

- **Status**: Production ready, all core modules implemented
- **Goal**: Native bilingual (Chinese/English) support, zero build dependencies
- **License**: MIT License (Copyright 2026 Aq!u)
- **Version**: 0.1.0

## Technology Stack

| Component | Choice | Notes |
| :--- | :--- | :--- |
| Language | Pure C (C11) | No C++ features; must compile with `-std=c11` |
| Markdown parser | [libcmark](https://github.com/commonmark/cmark) | Embedded in `src/cmark/`; excluded from git tracking |
| Config parser | [toml-c](https://github.com/cktan/toml-c) | Embedded as `src/toml.c` and `include/toml.h` |
| Memory allocator | Arena (region-based) | Single-header library `include/arena.h`; mandatory for core logic |
| Build tool | GNU Make | `Makefile` handles compilation, tests, and static analysis |
| Syntax highlighting | highlight.js | Loaded via CDN in default theme; no local assets |

All third-party dependencies are embedded. The resulting `cxo` binary has zero external library dependencies.

## Build and Test Commands

```bash
make              # Build cxo executable
make test         # Build and run all tests (stops on first failure)
make clean        # Remove build artifacts and test binaries
make distclean    # Also remove generated public/ directory
make dev          # Build then start dev server (no hot reload)
make check        # Run cppcheck and scan-build if available
make install      # Install cxo to $(PREFIX)/bin (default /usr/local/bin)
make uninstall    # Remove installed cxo binary
```

Platform detection in the Makefile:
- macOS → `CC = clang`
- Linux, MinGW, MSYS2 → `CC = gcc`

**Windows build** (MinGW-w64 or MSYS2):
```bash
make              # Build with MinGW gcc
make clean        # Uses cmd /c del on Windows, rm -f on POSIX
```

Windows-specific notes:
- Link with `-lws2_32` for Winsock (done automatically if using provided build commands)
- `cxo serve` uses Winsock2 on Windows; hot reload uses `_spawnvp` instead of `fork`/`exec`
- `cxo deploy` uses `where` instead of `which`, `xcopy` instead of `mv` for moving public files
- All POSIX-specific APIs are abstracted through `include/platform.h`

Compiler flags: `-Wall -Wextra -std=c11 -O2`. Dependency tracking is enabled via `-MMD -MP`.

## Code Organization

```text
.
├── include/           # Public headers
│   ├── cxo.h          # Core data structures and API
│   ├── cxo_error.h    # Error codes and CXO_IS_ERR macro
│   ├── arena.h        # Single-header arena allocator (STB-style)
│   ├── cmark.h        # libcmark public API
│   └── toml.h         # toml-c parser API
├── src/               # Source files
│   ├── main.c         # CLI entry point and command dispatch
│   ├── scanner.c      # Recursively scan one content/<lang>/ dir per language
│   ├── parser.c       # YAML frontmatter extraction, markdown→HTML, TOC generation
│   ├── linker.c       # Bilingual entry linking via hash table (djb2)
│   ├── lang.c         # Language descriptor table (cxo_lang_t): code/prefix/locale/label
│   ├── renderer.c     # cxo_render_site orchestrator only
│   ├── template.c     # Template loading, replace_var, escape_attr, fallback templates
│   ├── render_posts.c # Post pages, prev/next, lang switch, SEO meta tags
│   ├── render_index.c # Index pages + pagination
│   ├── render_taxonomy.c # Tag pages + year/month archive pages
│   ├── render_feeds.c # RSS feeds + sitemap
│   ├── path_util.c    # ensure_dir, file copy, cxo_entry_url, output paths
│   ├── renderer_internal.h # Cross-module declarations (private to src/)
│   ├── config.c       # TOML config parsing with defaults
│   ├── context.c      # cxo_context_t and cxo_entry_t allocation
│   ├── arena.c        # Arena implementation (defines ARENA_IMPLEMENTATION)
│   ├── toml.c         # Embedded toml-c implementation
│   ├── cmd_init.c     # `cxo init`, `cxo new`, `cxo clean`
│   ├── cmd_serve.c    # Development HTTP server with optional hot reload
│   ├── cmd_deploy.c   # GitHub Pages deployment logic
│   └── cmark/         # Embedded libcmark source files
├── tests/             # Integration tests
│   ├── test_scanner.c
│   ├── test_parser.c
│   ├── test_linker.c
│   ├── test_config.c
│   └── test_renderer.c
├── themes/default/    # HTML templates and CSS
│   ├── post.html
│   ├── index.html
│   ├── tag.html
│   ├── archive.html
│   └── style.css
└── content/           # Blog content
    ├── zh/            # Chinese posts
    └── en/            # English posts
```

## Architecture

The build pipeline follows a strict four-phase flow in `main.c`:

1. **Init**: Create arena allocator, create context, load `config.toml` (with hardcoded defaults).
2. **Scanner** (`scanner.c`): Recursively traverse `content/zh` and `content/en`, creating a `cxo_entry_t` per `.md` file. Slug defaults to filename without `.md`; `id` initially equals slug.
3. **Parser** (`parser.c`): For each entry, read the file, extract YAML frontmatter (`---` delimited), convert markdown body to HTML via `cmark_markdown_to_html()`, auto-generate table of contents with heading anchors, and auto-generate description excerpt from HTML if missing.
4. **Linker** (`linker.c`): Build a hash table (size 64, djb2 hash) keyed by `entry->id`. When two entries share the same `id` but have different `lang`, they are linked via the `peer` pointer bidirectionally.
5. **Renderer** (`renderer.c` orchestrator + `render_*.c` modules): Sort entries by date descending, assign `prev`/`next` chronologically within each language, then generate:
   - Individual post HTML pages (`public/posts/` and `public/en/posts/`)
   - Index pages with optional pagination
   - Tag aggregation pages (`public/tags/` and `public/en/tags/`)
   - Year and month archive pages (`public/YYYY/` and `public/YYYY/MM/`)
   - RSS feeds (`public/rss.xml`, `public/en/rss.xml`)
   - Sitemap (`public/sitemap.xml`)
   - Copy theme CSS and `static/` assets

### Core Data Structures

Defined in `include/cxo.h`:

```c
typedef struct cxo_entry {
    char* id;               /* Cross-language unique identifier */
    char* lang;             /* "zh" or "en" */
    char* title;
    char* date;             /* YYYY-MM-DD */
    char* slug;             /* URL path segment */
    char* html_content;     /* libcmark output, with TOC ids injected */
    char* md_content;       /* Original file path (not content) */
    struct cxo_entry* peer; /* Translation in other language, or NULL */
    int draft;              /* 1 = draft, 0 = published */
    char* description;      /* Excerpt; auto-generated from HTML if absent */
    char** tags;
    size_t tag_count;
    struct cxo_entry* prev; /* Chronologically older post (same lang) */
    struct cxo_entry* next; /* Chronologically newer post (same lang) */
    char* toc;              /* Table of contents HTML */
} cxo_entry_t;

typedef struct {
    cxo_entry_t** entries;
    size_t count;
    size_t capacity;
    char* base_url;
    char* theme_path;
    char* site_title;
    char* site_description;
    size_t posts_per_page;  /* 0 = no pagination */
} cxo_context_t;
```

### Memory Management

**Arena allocator is mandatory.** All core logic uses `arena_alloc()`, `arena_calloc()`, `arena_strdup()`, and `arena_alloc_array()`. The only exception is `free(html)` after `cmark_markdown_to_html()` returns a `malloc`-allocated string, which is immediately copied into the arena.

The arena is created once at program start (1 MiB default chunk size) and destroyed once at exit. There are no individual `free()` calls for entries, strings, or arrays during the pipeline.

## Code Style Guidelines

- **Linux Kernel Style variant**, 2 spaces, max 80 chars per line
- Pointer style: `char* str`, NOT `char *str`
- **All** control statements must use braces, even single-line
- Comments on separate lines only, never inline
- Functions under 50 lines, max 3-level nesting
- **Zero compiler warnings** enforced (`-Wall -Wextra`)
- Naming conventions:
  - Types: `snake_case_t`
  - Functions: `snake_case`
  - Macros: `UPPER_CASE`
  - Variables: `snake_case`

## Testing Strategy

Tests are integration-style binaries that exercise the real pipeline against the `content/` directory.

| Test | Validates |
| :--- | :--- |
| `test_scanner` | Directory traversal, entry creation, path correctness |
| `test_parser` | Frontmatter parsing, tag extraction, HTML generation, description |
| `test_linker` | Bilingual peer linking, bidirectional pointer integrity |
| `test_config` | Default values, TOML parsing, field population |
| `test_renderer` | File output (posts, tags, archives, index), prev/next nav, static asset copying |

All tests print `PASS`/`FAIL` lines and return non-zero on failure. `make test` runs them sequentially and stops at the first failure.

The renderer test creates temporary files under `static/` and `public/` and cleans them up on exit.

## CLI Commands

```bash
cxo init [dir]        # Create content/, themes/default/, config.toml, sample post
cxo new "Title"       # Create content/zh/<slug>.md with frontmatter template
cxo build             # Full build pipeline (alias: cxo g)
cxo serve [port]      # Dev server on port 8080 (alias: cxo s)
  -w, --watch         # Enable hot reload via SSE
cxo deploy            # Build and deploy to GitHub Pages via gh-pages branch
cxo clean             # Remove public/*
cxo version           # Show version (alias: cxo -v)
cxo help              # Show usage (alias: cxo -h)
```

### Dev Server (`cxo serve`)

- Serves `public/` over HTTP/1.1 using `select()` and `accept()`.
- Supports `GET` and `HEAD`.
- Directory listings generated on-the-fly for directories without `index.html`.
- URL decoding and directory-traversal protection (`..`) are implemented.
- Hot reload (`-w`): watches `content/`, `themes/`, and `config.toml` for mtime changes; rebuilds via `fork()` + `execlp("cxo", ...)`, then sends SSE `reload` event to connected browsers on `/__cxo_reload`.
- Hot reload script injection controlled by `CXO_HOTRELOAD` environment variable.

## Features

- **Bilingual routing**: Chinese (default) at `/posts/`, English at `/en/posts/`. The `peer` pointer drives `{{nav_lang_switch}}`.
- **Draft mode**: Skip drafts unless `CXO_DRAFT=1` is set in the environment.
- **Pagination**: Configurable `posts_per_page` in `config.toml`; `0` or omitted disables pagination.
- **Auto excerpts**: If `description` is absent, the renderer strips HTML tags and truncates to ~150 chars.
- **Table of Contents**: Automatically generated from HTML headings (h1–h6). Heading anchors are slugified; duplicates get numeric suffixes (`foo-2`, `foo-3`).
- **Template fallback**: If theme files are missing, hardcoded fallback templates in `template.c` are used.
- **Static assets**: `static/` directory is recursively copied to `public/` on every build.
- **SEO meta tags**: Open Graph and Twitter Card tags auto-generated per page via `{{meta_tags}}`.
- **Cross-platform**: Builds on Linux, macOS, and Windows (MinGW-w64/MSYS2) via `include/platform.h` abstraction layer
- **Dark mode toggle**: Theme-aware CSS with `localStorage` persistence and `prefers-color-scheme` respect; toggle button injected in all default templates and fallback templates.
- **RSS**: RFC-822 dates, XML-escaped titles, CDATA descriptions.
- **Sitemap**: Includes posts, paginated index pages, and archive pages for both languages.

## Content Format

Markdown files require YAML frontmatter; `id` links translations:

```markdown
---
id: hello
title: Article Title
date: 2026-03-19
description: Short excerpt for index page
tags: [tag1, tag2]
---

Content here...
```

- `slug` — URL slug; defaults to `id` if omitted
- `description` — Shown on index page and post page; auto-generated from content if omitted
- `tags` — Comma-separated list; generates tag pages and links on posts
- `draft` — Set to `true` to exclude from build (unless `CXO_DRAFT=1`)

## Templates

Template files live in `themes/default/`:

| Template | Purpose |
| :--- | :--- |
| `post.html` | Individual post pages |
| `index.html` | Homepage listing |
| `tag.html` | Tag aggregation pages |
| `archive.html` | Archive pages (falls back to `tag.html` if missing) |

### Template Variables

**Escaping contract** (enforced via `escape_attr()` in `template.c`, which
escapes `& < > "`):

- **Plain-text variables** — escaped before injection; themes must NOT
  escape them again: `title`, `date`, `description`, `site_title`,
  `site_description`, `tag_name`, and post titles/tag names inside
  `entry_list` / `prev` / `next` / `tags`.
- **Trusted HTML variables** — pre-rendered HTML injected raw; never put
  untrusted text into them: `content`, `toc`, `tags`, `nav_lang_switch`,
  `prev`, `next`, `entry_list`, `pagination`, `meta_tags`, `hotreload`.
- `{{lang}}` and `{{archive_title}}` are engine-generated from the language
  table / date strings and need no escaping.
- Note: slugs and tag names are inserted into `href` attributes HTML-escaped
  but NOT URL-encoded — keep them URL-safe (letters, digits, `-`, `_`,
  CJK characters).

**Common to all templates:**
- `{{site_title}}`, `{{site_description}}`, `{{lang}}`
- `{{hotreload}}` — injected in dev mode
- `{{meta_tags}}` — Open Graph + Twitter Card + description meta tags

**`post.html`:**
- `{{title}}`, `{{date}}`, `{{content}}`, `{{description}}`, `{{tags}}`
- `{{nav_lang_switch}}` — link to bilingual peer post
- `{{prev}}`, `{{next}}` — chronologically adjacent posts
- `{{toc}}` — table of contents HTML

**`index.html`:**
- `{{entry_list}}` — HTML list of posts
- `{{pagination}}` — newer/older navigation

**`tag.html`:**
- `{{tag_name}}` — current tag
- `{{entry_list}}` — posts for this tag

**`archive.html`:**
- `{{archive_title}}` — e.g. `2026` or `2026-03`
- `{{entry_list}}` — posts in the archive period

Templates use simple string substitution (`{{var}}` → value). No loops or conditionals inside templates; lists are pre-rendered HTML injected as single variables.

## Config

`config.toml` supports:

```toml
[site]
title = "My Blog"
description = "A minimalist blog powered by CXO"
base_url = "https://example.com"
posts_per_page = 10   # 0 or omit for no pagination

[theme]
path = "themes/default"
```

If `config.toml` is missing or unparsable, hardcoded defaults are used and execution continues.

## Error Codes

Defined in `include/cxo_error.h`. Use `CXO_IS_ERR(rc)` to check for errors.

| Code | Value | Meaning |
| :--- | :--- | :--- |
| `CXO_OK` | 0 | Success |
| `CXO_ERR_NOMEM` | -1 | Out of memory |
| `CXO_ERR_IO` | -2 | I/O error |
| `CXO_ERR_INVAL` | -3 | Invalid argument |
| `CXO_ERR_SCAN` | -10 | Scan failed |
| `CXO_ERR_NODIR` | -11 | Directory not found |
| `CXO_ERR_TOOMANY` | -12 | Too many entries (> 1024) |
| `CXO_ERR_PARSE` | -20 | Parse failed |
| `CXO_ERR_NOFILE` | -21 | File not found |
| `CXO_ERR_FMT` | -22 | Invalid format |
| `CXO_ERR_LINK` | -30 | Link failed |
| `CXO_ERR_DUPID` | -31 | Duplicate entry id in same language |
| `CXO_ERR_RENDER` | -40 | Render failed |
| `CXO_ERR_NOTHEME` | -41 | Theme not found |

## Security Considerations

- **Directory traversal**: The dev server rejects URIs containing `..` sequences (`has_traversal()` in `cmd_serve.c`).
- **Path length limits**: `MAX_PATH` (512), `MAX_OUTPUT_PATH` (4096), and `MAX_TEMPLATE_SIZE` (256 KiB) guard against unbounded path and template sizes.
- **Shell injection**: `cmd_deploy.c` uses `execvp()` for git commands where possible, avoiding shell interpolation of branch names and paths.
- **No user input in templates**: Template variables are literal string substitutions; there is no template expression evaluation that could execute code.

## Deployment

`cxo deploy` automates GitHub Pages deployment:

1. Verifies `git` is installed and the working directory is a git repo with a remote.
2. Runs `./cxo build` after cleaning `public/`.
3. Checks out (or creates) the `gh-pages` orphan branch.
4. Moves `public/*` to the repository root.
5. Commits with message `Deploy: <hash> from <branch>`.
6. Pushes to `origin gh-pages`.
7. Switches back to the original branch.

The deployment assumes the remote is GitHub and prints a warning if `github.com` is not found in the remote URL.

## Development Conventions

- **KISS**: Reject over-engineering. Prefer simple, readable algorithms.
- **Small & Atomic**: Functions should fit on one screen (~50 lines). If logic exceeds 3 nesting levels, extract a subfunction.
- **One responsibility**: If a function name needs "and" or "then", split it.
- **Git commits**: Lowercase imperative, one line. Atomic commits; ideal size 50–150 logical lines.
- **Self-review**: Before submitting changes, review the diff for debug prints or unrelated changes.
- **Branching**: New features on feature branches; `main`/`master` is stable.

## Notes

- `design.md` and `CODING_STYLE.md` contain detailed Chinese documentation that complements this guide.
- The `cxo` binary is excluded from Git tracking (see `.gitignore`).
- The dev server hot-reload rebuild spawns `cxo` via `fork()`/`execlp()`, so the binary must be in `PATH` or the current directory.
