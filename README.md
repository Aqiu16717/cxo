# CXO

[![CI](https://github.com/Aqiu16717/cxo/actions/workflows/ci.yml/badge.svg)](https://github.com/Aqiu16717/cxo/actions/workflows/ci.yml)

A minimalist, high-performance static blog engine written in pure C (C11).

- **Zero dependencies** — everything is embedded
- **Bilingual by design** — native Chinese/English support
- **Fast** — arena allocator, single-pass pipeline

## Build

```bash
make        # Build cxo executable
make test   # Run all tests
make ci     # Clean build and run all tests, as CI does
make clean  # Clean build artifacts
```

Requires a C11 compiler (clang or gcc) and GNU Make.

Every push and pull request is verified on Linux with GCC and Clang, macOS
with Clang, and Windows with MSYS2 MinGW-w64 UCRT64. Each job performs a clean
build, runs the complete test suite, and exercises the main CLI entry points.

## Quick Start

```bash
# Initialize a new blog
cxo init my-blog
cd my-blog

# Create a new post
cxo new "Hello World"

# Build the site
cxo build

# Serve locally with hot reload
cxo serve -w
```

## Content Format

Markdown files with YAML frontmatter live in `content/zh/` and `content/en/`.
The `id` field links translations together.

```markdown
---
id: hello
title: Hello World
date: 2026-03-21
description: Short excerpt for index and post pages
tags: [c, os]
---

Your content here...
```

| Field | Required | Description |
|-------|----------|-------------|
| `id` | Yes | Unique identifier; links bilingual posts |
| `title` | Yes | Post title |
| `date` | Yes | Publication date (`YYYY-MM-DD`) |
| `description` | No | Custom excerpt; shown on index and post pages; auto-generated from content if omitted |
| `tags` | No | Comma-separated tags; generates tag pages |
| `slug` | No | URL path; defaults to filename |
| `draft` | No | `true` to hide unless `CXO_DRAFT=1` |

## Templates

Place templates in `themes/default/`:

| Template | Purpose |
|----------|---------|
| `post.html` | Individual post pages |
| `index.html` | Homepage listing |
| `tag.html` | Tag aggregation pages |

Available template variables:

**Common**
- `{{site_title}}`, `{{site_description}}`, `{{lang}}`
- `{{hotreload}}` — injected in dev mode

**`post.html`**
- `{{title}}`, `{{date}}`, `{{content}}`, `{{description}}`, `{{tags}}`
- `{{nav_lang_switch}}` — link to the bilingual peer post
- `{{prev}}`, `{{next}}` — chronologically adjacent posts

**`index.html`**
- `{{entry_list}}` — HTML list of all posts

**`tag.html`**
- `{{tag_name}}` — name of the current tag
- `{{entry_list}}` — HTML list of posts for this tag

## Configuration

`config.toml`:

```toml
[site]
title = "My Blog"
description = "A minimalist blog"
base_url = "https://example.com"
posts_per_page = 10

[theme]
path = "themes/default"
```

## CLI Commands

```bash
cxo init [dir]       # Initialize project structure
cxo new "Title"      # Create a new post in content/zh/
cxo build            # Build the static site (alias: cxo g)
cxo serve [port]     # Dev server, default 8080 (alias: cxo s)
  -w, --watch        # Enable hot reload
cxo deploy           # Deploy to GitHub Pages
cxo clean            # Remove build artifacts
cxo version          # Show version (alias: cxo -v)
cxo help             # Show help (alias: cxo -h)
```

## Features

- **Bilingual routing** — `public/posts/` (zh) and `public/en/posts/` (en)
- **Tag system** — per-language tag pages at `/tags/<tag>.html`
- **Auto excerpts** — stripped from HTML content when `description` is absent
- **Prev / Next navigation** — chronologically linked posts
- **RSS feeds** — `public/rss.xml` and `public/en/rss.xml`
- **Sitemap** — `public/sitemap.xml`
- **Draft mode** — skip drafts unless `CXO_DRAFT=1`
- **Hot reload** — `cxo serve -w` watches content, themes, and config
- **Syntax highlighting** — highlight.js via CDN in default theme

## License

MIT License — Copyright (c) 2026 Aq!u
