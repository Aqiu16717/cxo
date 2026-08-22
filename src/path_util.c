/*
 * path_util.c - Filesystem helpers and entry URL construction
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include "renderer_internal.h"

/* Read file into arena */
char* read_file_to_arena(arena_t* arena, const char* path)
{
    FILE* fp;
    long size;
    char* content;
    
    fp = fopen(path, "r");
    if (!fp) {
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size <= 0 || size > MAX_TEMPLATE_SIZE) {
        fclose(fp);
        return NULL;
    }
    
    content = arena_alloc(arena, size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }
    
    fread(content, 1, size, fp);
    content[size] = '\0';
    fclose(fp);
    
    return content;
}

/* Copy file */
static int copy_file(const char* src, const char* dst)
{
    FILE* in;
    FILE* out;
    char buf[4096];
    size_t n;
    
    in = fopen(src, "rb");
    if (!in) {
        return CXO_ERR_IO;
    }
    
    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return CXO_ERR_IO;
    }
    
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }
    
    fclose(in);
    fclose(out);
    return CXO_OK;
}

/* Recursively copy directory contents from src to dst */
int copy_dir_recursive(const char* src, const char* dst)
{
    DIR* dir;
    struct dirent* entry;
    struct stat st;
    int rc;
    
    dir = opendir(src);
    if (!dir) {
        return CXO_ERR_IO;
    }
    
    rc = CXO_OK;
    while ((entry = readdir(dir)) != NULL && !CXO_IS_ERR(rc)) {
        char src_path[MAX_OUTPUT_PATH];
        char dst_path[MAX_OUTPUT_PATH];
        int n;
        
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        n = snprintf(src_path, sizeof(src_path),
                     "%s/%s", src, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(src_path)) {
            continue;
        }
        
        n = snprintf(dst_path, sizeof(dst_path),
                     "%s/%s", dst, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(dst_path)) {
            continue;
        }
        
        if (stat(src_path, &st) != 0) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            if (cxo_mkdir(dst_path) != 0 && errno != EEXIST) {
                rc = CXO_ERR_IO;
            } else {
                rc = copy_dir_recursive(src_path, dst_path);
            }
        } else if (S_ISREG(st.st_mode)) {
            rc = copy_file(src_path, dst_path);
        }
    }
    
    closedir(dir);
    return rc;
}

/* Copy static/ directory to output dir if it exists */
int copy_static_files(const char* output_dir)
{
    struct stat st;
    char dst[MAX_OUTPUT_PATH];
    int n;
    
    if (stat("static", &st) != 0 || !S_ISDIR(st.st_mode)) {
        return CXO_OK;
    }
    
    n = snprintf(dst, sizeof(dst), "%s", output_dir);
    if (n < 0 || (size_t)n >= sizeof(dst)) {
        return CXO_ERR_IO;
    }
    
    return copy_dir_recursive("static", dst);
}

/* Create directory */
int ensure_dir(const char* path)
{
    struct stat st;
    char parent[MAX_OUTPUT_PATH];
    char* last_slash;
    
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? CXO_OK : CXO_ERR_IO;
    }
    
    strncpy(parent, path, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    
    last_slash = strrchr(parent, '/');
    if (last_slash && last_slash != parent) {
        *last_slash = '\0';
        if (ensure_dir(parent) != CXO_OK) {
            return CXO_ERR_IO;
        }
    }
    
    if (cxo_mkdir(path) != 0 && errno != EEXIST) {
        return CXO_ERR_IO;
    }
    return CXO_OK;
}

/* Copy theme assets */
int copy_theme_assets(const char* theme_path, const char* output_dir)
{
    char src[MAX_OUTPUT_PATH];
    char dst[MAX_OUTPUT_PATH];
    int rc;
    
    /* Copy CSS */
    snprintf(src, sizeof(src), "%s/style.css", theme_path);
    snprintf(dst, sizeof(dst), "%s/style.css", output_dir);
    
    if (cxo_access(src, F_OK) == 0) {
        rc = ensure_dir(output_dir);
        if (CXO_IS_ERR(rc)) {
            return rc;
        }
        rc = copy_file(src, dst);
        if (CXO_IS_ERR(rc)) {
            fprintf(stderr, "Warning: Failed to copy style.css\n");
        }
    }
    
    return CXO_OK;
}

/* Get output subdirectory for a language's posts, e.g. "posts" or "en/posts".
 * Returns pointer to static buffer (same pattern as rfc822_date). */
const char* get_output_subdir(const char* lang)
{
    static char buf[64];
    const cxo_lang_t* l = cxo_lang_find(lang);

    if (l && l->prefix[0]) {
        snprintf(buf, sizeof(buf), "%s/posts", l->prefix);
    } else {
        snprintf(buf, sizeof(buf), "posts");
    }
    return buf;
}

/* Generate the canonical URL path for an entry,
 * e.g. "/posts/foo.html" or "/en/posts/foo.html".
 * Single authority for entry URL routing. */
char* cxo_entry_url(arena_t* arena, const cxo_entry_t* entry)
{
    const cxo_lang_t* l;
    char* url;
    size_t size;

    if (!entry || !entry->slug) {
        return arena_strdup(arena, "");
    }

    l = cxo_lang_find(entry->lang);
    size = strlen(entry->slug) + 32;
    url = arena_alloc(arena, size);
    if (!url) {
        return NULL;
    }
    if (l && l->prefix[0]) {
        snprintf(url, size, "/%s/posts/%s.html", l->prefix, entry->slug);
    } else {
        snprintf(url, size, "/posts/%s.html", entry->slug);
    }
    return url;
}
