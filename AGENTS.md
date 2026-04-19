# CXO - AI Agent Guide

**CXO** is a minimalist, high-performance static blog engine written in pure C (C11).

- **Status**: Production ready, all core modules implemented
- **Goal**: Native bilingual (Chinese/English) support, zero build dependencies
- **License**: MIT License (Copyright 2026 Aq!u)

## Build

```bash
make        # Build cxo executable
make test   # Build and run all tests
make clean  # Clean build artifacts
```

## Code Style

- **Linux Kernel Style**, 2 spaces, max 80 chars per line
- Pointer style: `char* str`, NOT `char *str`
- **All** control statements must use braces, even single-line
- Comments on separate lines only, not inline
- Arena allocator **mandatory**; do **not** use `malloc`/`free` in core logic
- Functions under 50 lines, max 3-level nesting
- **Zero compiler warnings** enforced

## Architecture

1. **Scanner**: Traverse `content/zh` and `content/en`
2. **Parser**: Extract YAML frontmatter, convert Markdown to HTML via libcmark
3. **Linker**: Link `cxo_entry_t` structs by `id` field for bilingual support
4. **Renderer**: Inject into HTML templates, output to `public/`

## CLI Commands

```bash
cxo init [dir]
cxo new "Title"
cxo build              # or: cxo g
cxo serve [port]       # dev server; use -w for hot reload
cxo deploy             # GitHub Pages deployment
cxo clean
cxo version            # or: cxo -v
```

## Features

- **Bilingual**: Hardcoded `zh`/`en` routing with `peer` entry linking
- **Templates**: `themes/default/post.html`, `themes/default/index.html`, and `themes/default/tag.html`
  - Template variables: `{{title}}`, `{{date}}`, `{{content}}`, `{{tags}}`, `{{description}}`, `{{lang}}`, `{{nav_lang_switch}}`, `{{site_title}}`, `{{site_description}}`, `{{hotreload}}`
  - Tag template additionally supports: `{{tag_name}}`, `{{entry_list}}`
- **Hot reload**: `cxo serve -w` watches `content/`, `themes/`, `config.toml`
- **Draft mode**: Skip drafts unless `CXO_DRAFT=1` is set
- **RSS**: Generates `public/rss.xml` and `public/en/rss.xml`
- **Sitemap**: Generates `public/sitemap.xml`
- **Syntax highlighting**: highlight.js via CDN in default theme

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

- `description` - Shown on index page and post page; auto-generated from content if omitted
- `tags` - Comma-separated list; generates tag pages and links on posts

## Error Codes

See `include/cxo_error.h`. Use `CXO_IS_ERR(rc)` to check for errors.

## Notes

- All third-party deps are embedded (`src/cmark/`, `src/toml.c`)
- `design.md` and `CODING_STYLE.md` contain detailed Chinese docs
- `cxo` binary is excluded from Git tracking
