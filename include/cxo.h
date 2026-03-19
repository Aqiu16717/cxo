/*
 * cxo.h - CXO Static Blog Engine Core Header
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#ifndef CXO_H
#define CXO_H

#include <stddef.h>
#include <stdbool.h>
#include "arena.h"

/* Version info */
#define CXO_VERSION "0.1.0"

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

/* Context and entry management */
cxo_context_t* cxo_context_create(arena_t* arena);
cxo_entry_t* cxo_entry_create(arena_t* arena);

/* File scanner - recursively scan content directory */
int cxo_scan_content(cxo_context_t* ctx, arena_t* arena, 
                     const char* content_dir);

/* Parser - parse markdown and frontmatter */
int cxo_parse_markdown(cxo_entry_t* entry, arena_t* arena,
                       const char* filepath);
int cxo_parse_frontmatter(cxo_entry_t* entry, arena_t* arena,
                          char* content, char** content_start);

/* Linker - associate bilingual entries by id */
int cxo_link_entries(cxo_context_t* ctx, arena_t* arena);

#endif /* CXO_H */
