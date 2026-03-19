/*
 * scanner.c - Content Directory Scanner
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include "../include/cxo.h"

#define MAX_PATH_LEN 4096

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
    /* Remove .md extension */
    slug = arena_alloc(arena, len - 2);
    if (!slug) {
        return NULL;
    }
    
    memcpy(slug, filename, len - 3);
    slug[len - 3] = '\0';
    
    return slug;
}

/* Recursively scan directory for markdown files */
static int scan_directory(cxo_context_t* ctx, arena_t* arena,
                          const char* dirpath, const char* lang)
{
    DIR* dir;
    struct dirent* entry;
    struct stat st;
    char fullpath[MAX_PATH_LEN];
    cxo_entry_t* cxo_entry;
    char* slug;
    
    dir = opendir(dirpath);
    if (!dir) {
        fprintf(stderr, "Warning: Cannot open directory %s: %s\n",
                dirpath, strerror(errno));
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
        
        if (stat(fullpath, &st) != 0) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            /* Recursively scan subdirectories */
            scan_directory(ctx, arena, fullpath, lang);
        } else if (S_ISREG(st.st_mode) && is_markdown_file(entry->d_name)) {
            /* Process markdown file */
            cxo_entry = cxo_entry_create(arena);
            if (!cxo_entry) {
                fprintf(stderr, "Error: Failed to create entry\n");
                continue;
            }
            
            cxo_entry->lang = arena_strdup(arena, lang);
            slug = extract_slug(arena, entry->d_name);
            cxo_entry->slug = slug;
            cxo_entry->id = slug; /* Use slug as id by default */
            
            /* Grow entries array if needed */
            if (ctx->count >= ctx->capacity) {
                cxo_entry_t** new_entries;
                size_t new_capacity;
                
                new_capacity = ctx->capacity == 0 ? 16 : ctx->capacity * 2;
                new_entries = realloc(ctx->entries, 
                                      new_capacity * sizeof(cxo_entry_t*));
                if (!new_entries) {
                    fprintf(stderr, "Error: Failed to grow entries array\n");
                    continue;
                }
                
                ctx->entries = new_entries;
                ctx->capacity = new_capacity;
            }
            
            /* Store full path for later parsing */
            cxo_entry->md_content = arena_strdup(arena, fullpath);
            ctx->entries[ctx->count++] = cxo_entry;
        }
    }
    
    closedir(dir);
    return 0;
}

/* Scan content directory for zh/ and en/ subdirectories */
int cxo_scan_content(cxo_context_t* ctx, arena_t* arena,
                     const char* content_dir)
{
    char path[MAX_PATH_LEN];
    struct stat st;
    
    /* Check if content directory exists */
    if (stat(content_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Content directory %s does not exist\n",
                content_dir);
        return -1;
    }
    
    /* Scan Chinese content */
    snprintf(path, sizeof(path), "%s/zh", content_dir);
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        scan_directory(ctx, arena, path, "zh");
    }
    
    /* Scan English content */
    snprintf(path, sizeof(path), "%s/en", content_dir);
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        scan_directory(ctx, arena, path, "en");
    }
    
    return 0;
}
