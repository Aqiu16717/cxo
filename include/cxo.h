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
#include "cxo_error.h"
#include "platform.h"

/* Version info */
#define CXO_VERSION "0.1.0"

/* Language descriptor - single source of truth for supported languages */
typedef struct {
    const char* code;    /* Language code, e.g. "zh" */
    const char* prefix;  /* URL prefix: "" for the default lang, else "en" */
    const char* locale;  /* Locale for og:locale, e.g. "zh_CN" */
    const char* label;   /* Display name for language switch, e.g. "中文" */
} cxo_lang_t;

#define CXO_MAX_LANGS 8

extern const cxo_lang_t CXO_LANGS[];
extern const size_t CXO_LANG_COUNT;

/* Look up a language by code; NULL if unknown */
const cxo_lang_t* cxo_lang_find(const char* code);

/* Table index of a language code; 0 (default language) if unknown */
size_t cxo_lang_index(const char* code);

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
    int draft;              /* Draft flag: 1 = draft, 0 = published */
    char* description;      /* Post description/excerpt */
    char** tags;            /* Tag array */
    size_t tag_count;       /* Number of tags */
    struct cxo_entry* prev; /* Previous post by date (same lang) */
    struct cxo_entry* next; /* Next post by date (same lang) */
    char* toc;              /* Table of contents HTML */
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
    size_t posts_per_page;  /* 0 = no pagination */
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

/* Linker - associate bilingual entries by id */
int cxo_link_entries(cxo_context_t* ctx, arena_t* arena);

/* Renderer - generate HTML site */
int cxo_render_site(cxo_context_t* ctx, arena_t* arena,
                    const char* output_dir);

/* Canonical URL path for an entry, e.g. "/posts/foo.html" or
 * "/en/posts/foo.html". Single authority for entry URL routing. */
char* cxo_entry_url(arena_t* arena, const cxo_entry_t* entry);

/* Config - load configuration from file */
int cxo_load_config(cxo_context_t* ctx, arena_t* arena,
                    const char* config_path);

#endif /* CXO_H */
