/*
 * scanner.c - Content Directory Scanner
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include "../include/platform.h"
#include "../include/cxo.h"

#define MAX_PATH_LEN 4096
#define MAX_ENTRIES 1024  /* Max blog entries, avoids realloc */

/* Forward declaration */
static int scan_directory(cxo_context_t* ctx, arena_t* arena,
                          const char* dirpath, const char* lang);

/* Check if filename ends with .md */
static bool is_markdown_file(const char* filename)
{
    size_t len;
    
    len = strlen(filename);
    if (len < 3) {
        return false;
    }
    
    return (filename[len - 3] == '.' && 
            filename[len - 2] == 'm' && 
            filename[len - 1] == 'd');
}

/* Extract slug from filename (remove .md extension) */
static char* extract_slug(arena_t* arena, const char* filename)
{
    char* slug;
    size_t len;
    
    len = strlen(filename);
    slug = arena_alloc(arena, len - 2);
    if (!slug) {
        return NULL;
    }
    
    memcpy(slug, filename, len - 3);
    slug[len - 3] = '\0';
    
    return slug;
}

/* Initialize entries array with arena */
static int init_entries_array(cxo_context_t* ctx, arena_t* arena)
{
    if (ctx->entries) {
        return CXO_OK;
    }
    
    ctx->entries = arena_calloc_count(arena, MAX_ENTRIES, sizeof(cxo_entry_t*));
    if (!ctx->entries) {
        return CXO_ERR_NOMEM;
    }
    
    ctx->capacity = MAX_ENTRIES;
    ctx->count = 0;
    return CXO_OK;
}

/* Create entry from markdown file */
static int create_entry_from_file(cxo_context_t* ctx, arena_t* arena,
                                  const char* fullpath, const char* filename,
                                  const char* lang)
{
    cxo_entry_t* entry;
    char* slug;
    
    if (ctx->count >= ctx->capacity) {
        fprintf(stderr, "Error: Too many entries (max %d)\n", MAX_ENTRIES);
        return CXO_ERR_TOOMANY;
    }
    
    entry = cxo_entry_create(arena);
    if (!entry) {
        fprintf(stderr, "Error: Failed to create entry\n");
        return CXO_ERR_NOMEM;
    }
    
    slug = extract_slug(arena, filename);
    if (!slug) {
        return CXO_ERR_NOMEM;
    }
    
    entry->lang = arena_strdup(arena, lang);
    entry->slug = slug;
    /* id defaults to the slug and shares its pointer; the parser replaces
     * id (never mutates it) when frontmatter provides one. Arena lifetime
     * makes the shared pointer safe. */
    entry->id = slug;
    entry->src_path = arena_strdup(arena, fullpath);
    
    ctx->entries[ctx->count++] = entry;
    return CXO_OK;
}

/* Process single directory entry */
static void process_dirent(cxo_context_t* ctx, arena_t* arena,
                           const char* dirpath, const char* name,
                           const char* lang)
{
    struct stat st;
    char fullpath[MAX_PATH_LEN];
    
    snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, name);
    
    if (stat(fullpath, &st) != 0) {
        return;
    }
    
    if (S_ISDIR(st.st_mode)) {
        scan_directory(ctx, arena, fullpath, lang);
    } else if (S_ISREG(st.st_mode) && is_markdown_file(name)) {
        create_entry_from_file(ctx, arena, fullpath, name, lang);
    }
}

/* Recursively scan directory for markdown files */
static int scan_directory(cxo_context_t* ctx, arena_t* arena,
                          const char* dirpath, const char* lang)
{
    DIR* dir;
    const struct dirent* entry;
    
    dir = opendir(dirpath);
    if (!dir) {
        fprintf(stderr, "Warning: Cannot open directory %s: %s\n",
                dirpath, strerror(errno));
        return CXO_ERR_SCAN;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        /* Skip hidden files */
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        process_dirent(ctx, arena, dirpath, entry->d_name, lang);
    }
    
    closedir(dir);
    return CXO_OK;
}

/* Scan language subdirectory */
static void scan_lang_dir(cxo_context_t* ctx, arena_t* arena,
                          const char* content_dir, const char* lang)
{
    char path[MAX_PATH_LEN];
    struct stat st;
    
    snprintf(path, sizeof(path), "%s/%s", content_dir, lang);
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        scan_directory(ctx, arena, path, lang);
    }
}

/* Scan content directory for one subdirectory per supported language */
int cxo_scan_content(cxo_context_t* ctx, arena_t* arena,
                     const char* content_dir)
{
    struct stat st;
    size_t i;
    int rc;

    /* Check if content directory exists */
    if (stat(content_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Content directory %s does not exist\n",
                content_dir);
        return CXO_ERR_NODIR;
    }

    /* Initialize entries array */
    rc = init_entries_array(ctx, arena);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Error: Failed to init entries array\n");
        return rc;
    }

    /* Scan every language directory */
    for (i = 0; i < CXO_LANG_COUNT; i++) {
        scan_lang_dir(ctx, arena, content_dir, CXO_LANGS[i].code);
    }

    return CXO_OK;
}
