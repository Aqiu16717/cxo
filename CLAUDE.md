# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

CXO is a minimalist static blog engine in pure C (C11), natively bilingual (Chinese/English), with zero external library dependencies — libcmark and toml-c are embedded in-tree (`src/cmark/`, `src/toml.c`).

**AGENTS.md contains the full project guide** (data structures, template variables, error codes, deployment). Read it for details not covered here.

## Commands

```bash
make            # Build cxo executable (clang on macOS, gcc elsewhere)
make test       # Build and run all tests, stops on first failure
make check      # Static analysis (cppcheck + scan-build, if installed)
make dev        # Build, then cxo build && cxo serve
make clean      # Remove objects and binaries (keeps public/)
make distclean  # Also remove generated public/
```

Run a single test:
```bash
make tests/test_parser && ./tests/test_parser
```

Tests are integration-style binaries that exercise the real pipeline against the repo's `content/` directory; they print PASS/FAIL lines and return non-zero on failure. They must run from the repo root.

## Architecture

Strict four-phase pipeline in `main.c`, driven by a `cxo_context_t` holding an array of `cxo_entry_t` (one per markdown file):

1. **Scanner** (`scanner.c`) — recursively walks `content/zh/` and `content/en/`, creating an entry per `.md` file.
2. **Parser** (`parser.c`) — extracts YAML frontmatter, converts markdown to HTML via embedded cmark, generates TOC with slugified heading anchors, auto-generates description excerpts.
3. **Linker** (`linker.c`) — hash table (size 64, djb2) keyed by frontmatter `id`; entries in different languages sharing an `id` are linked bidirectionally via `entry->peer`.
4. **Renderer** (`renderer.c`) — sorts by date, assigns `prev`/`next` within each language, emits posts, paginated indexes, tag pages, year/month archives, RSS, sitemap; copies theme CSS and `static/` assets into `public/`.

Key facts that span files:

- **Bilingual routing**: zh is the default at `/posts/`, en lives under `/en/`. The `peer` pointer drives the `{{nav_lang_switch}}` template variable.
- **Templates** are pure `{{var}}` string substitution — no loops/conditionals; lists are pre-rendered HTML injected as single variables. Missing theme files fall back to hardcoded templates in `renderer.c`.
- **Drafts** are skipped unless `CXO_DRAFT=1`; hot-reload script injection is controlled by `CXO_HOTRELOAD`.
- **Config** (`config.toml`) is optional — missing or unparsable files fall back to hardcoded defaults.

## Mandatory conventions

- **Arena allocator is mandatory** for all core logic (`arena_alloc`, `arena_calloc`, `arena_strdup` — see `include/arena.h`). Created once at start, destroyed at exit; no individual `free()` in the pipeline. The only exception is freeing cmark's `malloc`-ed HTML immediately after copying it into the arena. Never use raw `malloc`/`free` in core logic.
- **Zero compiler warnings** (`-Wall -Wextra`); treat warnings as errors.
- **Style**: Linux Kernel style variant — 2-space indent, 80-char lines, `char* str` (not `char *str`), braces on all control statements, comments on their own line (never trailing), types `snake_case_t`, functions/vars `snake_case`, macros `UPPER_CASE`.
- **Functions**: ~50 lines max, ≤3 nesting levels, single responsibility — extract subfunctions when deeper.
- **Error codes**: use the codes in `include/cxo_error.h` and `CXO_IS_ERR(rc)`; never hardcode numbers.
- **Commits**: lowercase imperative, one line, atomic (50–150 logical lines ideal). Features go on feature branches; `main` stays stable.
- `CODING_STYLE.md` and `design.md` contain the detailed (Chinese) style and design docs.

## Platform notes

Cross-platform (Linux/macOS/Windows MinGW/MSYS2); POSIX-specific APIs are abstracted in `include/platform.h`. On Windows, link `-lws2_32`; hot reload uses `_spawnvp` instead of `fork`/`exec`. The hot-reload rebuild spawns `cxo` via `fork()`/`execlp()`, so the binary must be in `PATH` or the current directory.
