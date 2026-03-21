/*
 * renderer.c - HTML Template Renderer
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "../include/cxo.h"

#define MAX_OUTPUT_PATH 4096

/* Default HTML template */
static const char* default_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "<title>{{title}}</title>\n"
    "<style>\n"
    "body{font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",Helvetica,Arial,sans-serif;max-width:800px;margin:40px auto;padding:0 20px;line-height:1.6;color:#333}\n"
    "h1,h2,h3{color:#222}\n"
    "nav{margin-bottom:30px;padding-bottom:20px;border-bottom:1px solid #eee}\n"
    "nav a{color:#0366d6;text-decoration:none;margin-right:20px}\n"
    ".meta{color:#666;font-size:0.9em;margin-bottom:20px}\n"
    "footer{margin-top:40px;padding-top:20px;border-top:1px solid #eee;color:#666;font-size:0.9em}\n"
    "</style>\n"
    "</head>\n"
    "<body>\n"
    "<nav>\n"
    "<a href=\"/\">Home</a>\n"
    "{{nav_lang_switch}}\n"
    "</nav>\n"
    "<article>\n"
    "<h1>{{title}}</h1>\n"
    "<div class=\"meta\">{{date}}</div>\n"
    "<div class=\"content\">\n"
    "{{content}}\n"
    "</div>\n"
    "</article>\n"
    "<footer>\n"
    "<p>Powered by CXO</p>\n"
    "</footer>\n"
    "</body>\n"
    "</html>\n";

/* Count occurrences of substring in string */
static size_t count_substr(const char* str, const char* sub)
{
    size_t count;
    size_t sub_len;
    char* p;
    
    count = 0;
    sub_len = strlen(sub);
    p = (char*)str;
    
    while ((p = strstr(p, sub)) != NULL) {
        count++;
        p += sub_len;
    }
    
    return count;
}

/* Replace all occurrences in result buffer */
static void do_replace(char* result, const char* tmpl,
                       const char* placeholder, const char* value)
{
    char* pos;
    char* last;
    size_t ph_len;
    
    ph_len = strlen(placeholder);
    pos = (char*)tmpl;
    last = (char*)tmpl;
    result[0] = '\0';
    
    while ((pos = strstr(pos, placeholder)) != NULL) {
        strncat(result, last, pos - last);
        strcat(result, value);
        pos += ph_len;
        last = pos;
    }
    strcat(result, last);
}

/* Replace template variable {{name}} with value */
static char* replace_var(arena_t* arena, const char* tmpl,
                         const char* name, const char* value)
{
    char placeholder[64];
    size_t ph_len;
    size_t count;
    size_t result_len;
    char* result;
    
    /* Build placeholder {{name}} */
    snprintf(placeholder, sizeof(placeholder), "{{%s}}", name);
    ph_len = strlen(placeholder);
    
    value = value ? value : "";
    
    /* Count occurrences */
    count = count_substr(tmpl, placeholder);
    if (count == 0) {
        return arena_strdup(arena, tmpl);
    }
    
    /* Calculate result size */
    result_len = strlen(tmpl) + count * (strlen(value) - ph_len) + 1;
    result = arena_alloc(arena, result_len);
    if (!result) {
        return NULL;
    }
    
    /* Replace all occurrences */
    do_replace(result, tmpl, placeholder, value);
    
    return result;
}

/* Create directory recursively */
static int ensure_dir(const char* path)
{
    struct stat st;
    char parent[MAX_OUTPUT_PATH];
    char* last_slash;
    
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? CXO_OK : CXO_ERR_IO;
    }
    
    /* Try to create parent first */
    strncpy(parent, path, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    
    last_slash = strrchr(parent, '/');
    if (last_slash && last_slash != parent) {
        *last_slash = '\0';
        if (ensure_dir(parent) != CXO_OK) {
            return CXO_ERR_IO;
        }
    }
    
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        return CXO_ERR_IO;
    }
    
    return CXO_OK;
}

/* Generate language switch link */
static char* build_lang_switch(arena_t* arena, cxo_entry_t* entry)
{
    char buf[256];
    const char* url;
    const char* label;
    
    if (!entry->peer) {
        return arena_strdup(arena, "");
    }
    
    if (strcmp(entry->peer->lang, "en") == 0) {
        url = "/en/posts/";
        label = "English";
    } else {
        url = "/posts/";
        label = "中文";
    }
    
    snprintf(buf, sizeof(buf), "<a href=\"%s%s.html\">%s</a>",
             url, entry->peer->slug, label);
    
    return arena_strdup(arena, buf);
}

/* Get output subdirectory based on language */
static const char* get_output_subdir(const char* lang)
{
    return (strcmp(lang, "en") == 0) ? "en/posts" : "posts";
}

/* Build output path */
static void build_output_path(char* buf, size_t size,
                              const char* output_dir,
                              const char* subdir, const char* slug)
{
    snprintf(buf, size, "%s/%s/%s.html", output_dir, subdir, slug);
}

/* Generate HTML from template with entry data */
static char* generate_html(arena_t* arena, cxo_entry_t* entry)
{
    char* html;
    char* tmpl;
    char* lang_switch;
    
    lang_switch = build_lang_switch(arena, entry);
    tmpl = arena_strdup(arena, default_template);
    
    html = replace_var(arena, tmpl, "title", entry->title);
    html = replace_var(arena, html, "date", entry->date);
    html = replace_var(arena, html, "lang", entry->lang);
    html = replace_var(arena, html, "content", entry->html_content);
    html = replace_var(arena, html, "nav_lang_switch", lang_switch);
    
    return html;
}

/* Write HTML to file */
static int write_html_file(const char* path, const char* html)
{
    FILE* fp;
    
    fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot write %s\n", path);
        return CXO_ERR_IO;
    }
    
    fprintf(fp, "%s", html);
    fclose(fp);
    return CXO_OK;
}

/* Render single entry to HTML file */
static int render_entry(cxo_entry_t* entry, arena_t* arena,
                        const char* output_dir)
{
    char path[MAX_OUTPUT_PATH];
    const char* subdir;
    char* html;
    int rc;
    
    subdir = get_output_subdir(entry->lang);
    
    /* Create output directory */
    snprintf(path, sizeof(path), "%s/%s", output_dir, subdir);
    rc = ensure_dir(path);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Error: Cannot create directory %s\n", path);
        return rc;
    }
    
    /* Generate and write HTML */
    html = generate_html(arena, entry);
    build_output_path(path, sizeof(path), output_dir, subdir, entry->slug);
    
    rc = write_html_file(path, html);
    if (!CXO_IS_ERR(rc)) {
        printf("Generated: %s\n", path);
    }
    
    return rc;
}

/* Render entire site */
int cxo_render_site(cxo_context_t* ctx, arena_t* arena,
                    const char* output_dir)
{
    size_t i;
    int rc;
    int success;
    
    /* Create output directory */
    rc = ensure_dir(output_dir);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Error: Cannot create output directory\n");
        return rc;
    }
    
    success = 0;
    for (i = 0; i < ctx->count; i++) {
        rc = render_entry(ctx->entries[i], arena, output_dir);
        if (!CXO_IS_ERR(rc)) {
            success++;
        }
    }
    
    printf("Rendered %d/%zu entries\n", success, ctx->count);
    
    return (success == (int)ctx->count) ? CXO_OK : CXO_ERR_RENDER;
}
