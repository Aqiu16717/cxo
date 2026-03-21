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
#define MAX_TEMPLATE_SIZE (256 * 1024)

/* Fallback inline template */
static const char* fallback_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<title>{{title}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a> {{nav_lang_switch}}</nav>\n"
    "<article>\n"
    "<h1>{{title}}</h1>\n"
    "<div class=\"meta\">{{date}}</div>\n"
    "<div class=\"content\">{{content}}</div>\n"
    "</article>\n"
    "<footer><p>{{site_description}}</p></footer>\n"
    "</body>\n"
    "</html>\n";

/* Read file into arena */
static char* read_file_to_arena(arena_t* arena, const char* path)
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
    
    in = fopen(src, "r");
    if (!in) {
        return CXO_ERR_IO;
    }
    
    out = fopen(dst, "w");
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

/* Count occurrences */
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

/* Replace occurrences */
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

/* Replace template variable */
static char* replace_var(arena_t* arena, const char* tmpl,
                         const char* name, const char* value)
{
    char placeholder[64];
    size_t ph_len;
    size_t count;
    size_t result_len;
    char* result;
    
    snprintf(placeholder, sizeof(placeholder), "{{%s}}", name);
    ph_len = strlen(placeholder);
    value = value ? value : "";
    
    count = count_substr(tmpl, placeholder);
    if (count == 0) {
        return arena_strdup(arena, tmpl);
    }
    
    result_len = strlen(tmpl) + count * (strlen(value) - ph_len) + 1;
    result = arena_alloc(arena, result_len);
    if (!result) {
        return NULL;
    }
    
    do_replace(result, tmpl, placeholder, value);
    return result;
}

/* Create directory */
static int ensure_dir(const char* path)
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
    
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        return CXO_ERR_IO;
    }
    return CXO_OK;
}

/* Build language switch */
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

/* Get output subdirectory */
static const char* get_output_subdir(const char* lang)
{
    return (strcmp(lang, "en") == 0) ? "en/posts" : "posts";
}

/* Load template */
static char* load_template(arena_t* arena, const char* theme_path)
{
    char path[MAX_OUTPUT_PATH];
    char* tmpl;
    
    snprintf(path, sizeof(path), "%s/post.html", theme_path);
    tmpl = read_file_to_arena(arena, path);
    if (!tmpl) {
        return arena_strdup(arena, fallback_template);
    }
    return tmpl;
}

/* Copy theme assets */
static int copy_theme_assets(const char* theme_path, const char* output_dir)
{
    char src[MAX_OUTPUT_PATH];
    char dst[MAX_OUTPUT_PATH];
    int rc;
    
    /* Copy CSS */
    snprintf(src, sizeof(src), "%s/style.css", theme_path);
    snprintf(dst, sizeof(dst), "%s/style.css", output_dir);
    
    if (access(src, F_OK) == 0) {
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

/* Generate HTML */
static char* generate_html(cxo_entry_t* entry, cxo_context_t* ctx,
                           arena_t* arena, const char* tmpl)
{
    char* html;
    char* lang_switch;
    
    lang_switch = build_lang_switch(arena, entry);
    
    html = replace_var(arena, tmpl, "title", entry->title);
    html = replace_var(arena, html, "date", entry->date);
    html = replace_var(arena, html, "lang", entry->lang);
    html = replace_var(arena, html, "content", entry->html_content);
    html = replace_var(arena, html, "nav_lang_switch", lang_switch);
    html = replace_var(arena, html, "site_title", ctx->site_title);
    html = replace_var(arena, html, "site_description", ctx->site_description);
    
    return html;
}

/* Write HTML file */
static int write_html(const char* path, const char* html)
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

/* Render single entry */
static int render_entry(cxo_entry_t* entry, cxo_context_t* ctx,
                        arena_t* arena, const char* output_dir,
                        const char* tmpl)
{
    char path[MAX_OUTPUT_PATH];
    const char* subdir;
    char* html;
    int rc;
    
    subdir = get_output_subdir(entry->lang);
    
    snprintf(path, sizeof(path), "%s/%s", output_dir, subdir);
    rc = ensure_dir(path);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Error: Cannot create directory %s\n", path);
        return rc;
    }
    
    html = generate_html(entry, ctx, arena, tmpl);
    snprintf(path, sizeof(path), "%s/%s/%s.html",
             output_dir, subdir, entry->slug);
    
    rc = write_html(path, html);
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
    char* tmpl;
    
    rc = ensure_dir(output_dir);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Error: Cannot create output directory\n");
        return rc;
    }
    
    /* Load template and copy assets */
    tmpl = load_template(arena, ctx->theme_path);
    copy_theme_assets(ctx->theme_path, output_dir);
    
    success = 0;
    for (i = 0; i < ctx->count; i++) {
        rc = render_entry(ctx->entries[i], ctx, arena, output_dir, tmpl);
        if (!CXO_IS_ERR(rc)) {
            success++;
        }
    }
    
    printf("Rendered %d/%zu entries\n", success, ctx->count);
    
    return (success == (int)ctx->count) ? CXO_OK : CXO_ERR_RENDER;
}
