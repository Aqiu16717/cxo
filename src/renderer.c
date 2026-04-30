/*
 * renderer.c - HTML Template Renderer
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include "../include/cxo.h"

#define MAX_OUTPUT_PATH 4096
#define MAX_TEMPLATE_SIZE (256 * 1024)
#define EXCERPT_MAX_LEN 150
#define MAX_ARCHIVE_PERIODS 128

/* Hot reload script - injected when CXO_HOTRELOAD=1 */
static const char* hotreload_script =
    "<script>\n"
    "(function() {\n"
    "  const es = new EventSource('/__cxo_reload');\n"
    "  es.onmessage = function(e) {\n"
    "    if (e.data === 'reload') {\n"
    "      console.log('[CXO] Reloading...');\n"
    "      location.reload();\n"
    "    }\n"
    "  };\n"
    "  es.onerror = function() {\n"
    "    console.log('[CXO] Hot reload disconnected');\n"
    "  };\n"
    "})();\n"
    "</script>\n";

/* Fallback archive template */
static const char* fallback_archive_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<title>{{site_title}} - {{archive_title}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a></nav>\n"
    "<h1>{{archive_title}}</h1>\n"
    "<ul class=\"post-list\">\n"
    "{{entry_list}}"
    "</ul>\n"
    "{{hotreload}}"
    "</body>\n"
    "</html>\n";

/* Fallback tag template */
static const char* fallback_tag_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<title>{{site_title}} - {{tag_name}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a></nav>\n"
    "<h1>{{tag_name}}</h1>\n"
    "<ul class=\"post-list\">\n"
    "{{entry_list}}"
    "</ul>\n"
    "{{hotreload}}"
    "</body>\n"
    "</html>\n";

/* Fallback index template */
static const char* fallback_index_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<title>{{site_title}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a></nav>\n"
    "<h1>{{site_title}}</h1>\n"
    "<ul class=\"post-list\">\n"
    "{{entry_list}}"
    "</ul>\n"
    "{{pagination}}"
    "{{hotreload}}"
    "</body>\n"
    "</html>\n";

/* Fallback inline template */
static const char* fallback_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<title>{{title}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github.min.css\">\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a> {{nav_lang_switch}}</nav>\n"
    "<article>\n"
    "<h1>{{title}}</h1>\n"
    "<div class=\"meta\">{{date}}</div>\n"
    "<div class=\"tags\">{{tags}}</div>\n"
    "<div class=\"description\">{{description}}</div>\n"
    "<div class=\"content\">{{content}}</div>\n"
    "<nav class=\"post-nav\">{{prev}}{{next}}</nav>\n"
    "</article>\n"
    "<footer><p>{{site_description}}</p></footer>\n"
    "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js\"></script>\n"
    "<script>hljs.highlightAll();</script>\n"
    "{{hotreload}}"
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
static int copy_dir_recursive(const char* src, const char* dst)
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
        
        if (entry->d_name[0] == '.' ||
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
            if (mkdir(dst_path, 0755) != 0 && errno != EEXIST) {
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
static int copy_static_files(const char* output_dir)
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
    
    {
        size_t value_len = strlen(value);
        size_t extra = (value_len > ph_len) ? (value_len - ph_len) : 0;
        result_len = strlen(tmpl) + count * extra + 1;
    }
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
static char* build_lang_switch(arena_t* arena, const cxo_entry_t* entry)
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

/* Build prev/next navigation link */
static char* build_nav_link(arena_t* arena, cxo_entry_t* entry,
                            const char* rel_class)
{
    char buf[512];
    const char* subdir;
    
    if (!entry) {
        return arena_strdup(arena, "");
    }
    
    subdir = get_output_subdir(entry->lang);
    snprintf(buf, sizeof(buf),
             "<a href=\"/%s/%s.html\" class=\"%s\">%s</a>",
             subdir, entry->slug, rel_class, entry->title);
    return arena_strdup(arena, buf);
}

/* Compare entries by date descending, then by slug */
static int compare_entries_by_date(const void* a, const void* b)
{
    const cxo_entry_t* ea = *(const cxo_entry_t**)a;
    const cxo_entry_t* eb = *(const cxo_entry_t**)b;
    int cmp = strcmp(eb->date, ea->date);
    if (cmp != 0) {
        return cmp;
    }
    return strcmp(ea->slug, eb->slug);
}

/* Sort entries by date descending */
static void sort_entries(cxo_context_t* ctx)
{
    if (ctx->count > 1) {
        qsort(ctx->entries, ctx->count, sizeof(cxo_entry_t*),
              compare_entries_by_date);
    }
}

/* Assign prev/next post pointers by date within same language.
 * Entries must be sorted by date descending before calling.
 * Runs in O(n) by tracking last seen entry per language.
 */
static void assign_prev_next(cxo_context_t* ctx)
{
    size_t i;
    cxo_entry_t* last_zh = NULL;
    cxo_entry_t* last_en = NULL;
    
    /* First pass (newest to oldest): assign next (newer) pointers */
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        cxo_entry_t** last = (strcmp(entry->lang, "en") == 0) ? &last_en : &last_zh;
        
        entry->next = *last;
        entry->prev = NULL;
        *last = entry;
    }
    
    /* Second pass (oldest to newest): assign prev (older) pointers */
    last_zh = NULL;
    last_en = NULL;
    for (i = ctx->count; i > 0; i--) {
        cxo_entry_t* entry = ctx->entries[i - 1];
        cxo_entry_t** last = (strcmp(entry->lang, "en") == 0) ? &last_en : &last_zh;
        
        entry->prev = *last;
        *last = entry;
    }
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

/* Load tag template */
static char* load_tag_template(arena_t* arena, const char* theme_path)
{
    char path[MAX_OUTPUT_PATH];
    char* tmpl;
    
    snprintf(path, sizeof(path), "%s/tag.html", theme_path);
    tmpl = read_file_to_arena(arena, path);
    if (!tmpl) {
        return arena_strdup(arena, fallback_tag_template);
    }
    return tmpl;
}

/* Load index template */
static char* load_index_template(arena_t* arena, const char* theme_path)
{
    char path[MAX_OUTPUT_PATH];
    char* tmpl;
    
    snprintf(path, sizeof(path), "%s/index.html", theme_path);
    tmpl = read_file_to_arena(arena, path);
    if (!tmpl) {
        return arena_strdup(arena, fallback_index_template);
    }
    return tmpl;
}

/* Load archive template */
static char* load_archive_template(arena_t* arena, const char* theme_path)
{
    char path[MAX_OUTPUT_PATH];
    char* tmpl;
    
    snprintf(path, sizeof(path), "%s/archive.html", theme_path);
    tmpl = read_file_to_arena(arena, path);
    if (!tmpl) {
        snprintf(path, sizeof(path), "%s/tag.html", theme_path);
        tmpl = read_file_to_arena(arena, path);
        if (!tmpl) {
            return arena_strdup(arena, fallback_archive_template);
        }
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

/* Check if hot reload is enabled */
static int hotreload_enabled(void)
{
    return getenv("CXO_HOTRELOAD") != NULL;
}

/* Forward declarations */
static char* build_tag_links(cxo_entry_t* entry, arena_t* arena);

/* Generate HTML */
static char* generate_html(cxo_entry_t* entry, const cxo_context_t* ctx,
                           arena_t* arena, const char* tmpl)
{
    char* html;
    char* lang_switch;
    char* tag_links;
    const char* description;
    
    lang_switch = build_lang_switch(arena, entry);
    tag_links = build_tag_links(entry, arena);
    description = entry->description ? entry->description : "";
    
    html = replace_var(arena, tmpl, "title", entry->title);
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "date", entry->date);
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "lang", entry->lang);
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "content", entry->html_content);
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "nav_lang_switch", lang_switch);
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "tags", tag_links);
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "description", description);
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "prev",
                       build_nav_link(arena, entry->prev, "prev"));
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "next",
                       build_nav_link(arena, entry->next, "next"));
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "site_title", ctx->site_title);
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "site_description", ctx->site_description);
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "toc", entry->toc ? entry->toc : "");
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "hotreload",
                       hotreload_enabled() ? hotreload_script : "");
    
    return html;
}

/* Write HTML file */
static int write_html(const char* path, const char* html)
{
    FILE* fp;
    size_t len;
    size_t written;
    
    fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot write %s\n", path);
        return CXO_ERR_IO;
    }
    
    len = strlen(html);
    written = fprintf(fp, "%s", html);
    fclose(fp);
    
    if (written != len) {
        fprintf(stderr, "Error: Failed to write %s (disk full?)\n", path);
        return CXO_ERR_IO;
    }
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
    
    /* Skip draft posts unless CXO_DRAFT=1 */
    if (entry->draft && getenv("CXO_DRAFT") == NULL) {
        return CXO_OK;
    }
    
    subdir = get_output_subdir(entry->lang);
    
    snprintf(path, sizeof(path), "%s/%s", output_dir, subdir);
    rc = ensure_dir(path);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Error: Cannot create directory %s\n", path);
        return rc;
    }
    
    html = generate_html(entry, ctx, arena, tmpl);
    if (!html) {
        fprintf(stderr, "Error: Failed to generate HTML for %s\n", entry->slug);
        return CXO_ERR_RENDER;
    }
    
    snprintf(path, sizeof(path), "%s/%s/%s.html",
             output_dir, subdir, entry->slug);
    
    rc = write_html(path, html);
    if (!CXO_IS_ERR(rc)) {
        printf("Generated: %s\n", path);
    }
    return rc;
}

/* Check if draft posts should be rendered */
static int show_drafts(void)
{
    return getenv("CXO_DRAFT") != NULL;
}

/* Count entries matching language and draft filter */
static size_t count_matching_entries(cxo_context_t* ctx, const char* lang,
                                     int include_drafts)
{
    size_t i;
    size_t count = 0;
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        if (strcmp(entry->lang, lang) == 0 && (!entry->draft || include_drafts)) {
            count++;
        }
    }
    return count;
}

/* Calculate buffer size needed for entry list HTML */
static size_t calc_list_len(cxo_context_t* ctx, const char* lang,
                            int include_drafts)
{
    size_t i;
    size_t total_len = 256;
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        if (strcmp(entry->lang, lang) != 0 || (entry->draft && !include_drafts)) {
            continue;
        }
        total_len += 300 + strlen(entry->slug) + strlen(entry->title) +
                     strlen(entry->date);
    }
    return total_len;
}

/* Build excerpt from description or HTML content */
static char* build_excerpt(cxo_entry_t* entry, arena_t* arena)
{
    const char* src;
    char* excerpt;
    size_t src_len;
    size_t i;
    size_t j;
    int in_tag;
    
    if (entry->description && strlen(entry->description) > 0) {
        return arena_strdup(arena, entry->description);
    }
    
    src = entry->html_content ? entry->html_content : "";
    src_len = strlen(src);
    
    excerpt = arena_alloc(arena, src_len + 4);
    if (!excerpt) {
        return NULL;
    }
    
    j = 0;
    in_tag = 0;
    for (i = 0; i < src_len && j < EXCERPT_MAX_LEN; i++) {
        if (src[i] == '<') {
            in_tag = 1;
        } else if (src[i] == '>') {
            in_tag = 0;
        } else if (!in_tag) {
            excerpt[j++] = src[i];
        }
    }
    excerpt[j] = '\0';
    
    if (i < src_len) {
        if (j > 0 && !isspace((unsigned char)excerpt[j - 1])) {
            size_t k = j;
            while (k > 0 && k > j - 20) {
                if (isspace((unsigned char)excerpt[k - 1])) {
                    break;
                }
                k--;
            }
            if (k > 0) {
                j = k;
            }
        }
        excerpt[j] = '\0';
        strcpy(excerpt + j, "...");
    }
    
    while (j > 0 && isspace((unsigned char)excerpt[j - 1])) {
        excerpt[--j] = '\0';
    }
    
    return excerpt;
}

/* Build tag links HTML for a post */
static char* build_tag_links(cxo_entry_t* entry, arena_t* arena)
{
    size_t i;
    size_t total_len;
    char* buf;
    size_t offset;
    const char* tag_prefix;
    
    if (entry->tag_count == 0) {
        return arena_strdup(arena, "");
    }
    
    tag_prefix = (strcmp(entry->lang, "en") == 0) ? "/en/tags/" : "/tags/";
    
    total_len = 32;
    for (i = 0; i < entry->tag_count; i++) {
        total_len += strlen(tag_prefix) + strlen(entry->tags[i]) * 2 + 32;
    }
    
    buf = arena_alloc(arena, total_len);
    if (!buf) {
        return NULL;
    }
    
    offset = 0;
    
    for (i = 0; i < entry->tag_count; i++) {
        if (i > 0) {
            offset += snprintf(buf + offset, total_len - offset, " ");
        }
        offset += snprintf(buf + offset, total_len - offset,
                           "<a href=\"%s%s.html\">%s</a>",
                           tag_prefix, entry->tags[i], entry->tags[i]);
    }
    
    return buf;
}

/* Append a single entry link to the list buffer */
static void append_entry_link(char* buf, size_t total_len, size_t* offset,
                              cxo_entry_t* entry, arena_t* arena)
{
    const char* subdir = get_output_subdir(entry->lang);
    char* excerpt;
    int written;
    
    excerpt = build_excerpt(entry, arena);
    if (!excerpt) {
        excerpt = "";
    }
    
    written = snprintf(buf + *offset, total_len - *offset,
                       "<li><a href=\"/%s/%s.html\">%s</a> <span class=\"date\">%s</span><div class=\"excerpt\">%s</div></li>\n",
                       subdir, entry->slug, entry->title, entry->date,
                       excerpt);
    if (written > 0) {
        *offset += written;
    }
}

/* Build entry list HTML for index page */
static char* build_entry_list(cxo_context_t* ctx, arena_t* arena, const char* lang)
{
    size_t i;
    size_t total_len;
    char* list_html;
    size_t offset;
    size_t count;
    int include_drafts;
    
    include_drafts = show_drafts();
    count = count_matching_entries(ctx, lang, include_drafts);
    
    if (count == 0) {
        return arena_strdup(arena, "<li>No posts yet</li>\n");
    }
    
    total_len = calc_list_len(ctx, lang, include_drafts);
    list_html = arena_alloc(arena, total_len);
    if (!list_html) {
        return NULL;
    }
    
    list_html[0] = '\0';
    offset = 0;
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        if (strcmp(entry->lang, lang) != 0 || (entry->draft && !include_drafts)) {
            continue;
        }
        append_entry_link(list_html, total_len, &offset, entry, arena);
    }
    
    return list_html;
}

/* Build paginated entry list for a specific page */
static char* build_paginated_entry_list(cxo_context_t* ctx, arena_t* arena,
                                        const char* lang, size_t page,
                                        size_t per_page)
{
    size_t i;
    size_t match_count = 0;
    size_t start = (page - 1) * per_page;
    size_t end = start + per_page;
    size_t total_len = 256;
    char* list_html;
    size_t offset = 0;
    int include_drafts = show_drafts();
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        if (strcmp(entry->lang, lang) != 0 || (entry->draft && !include_drafts)) {
            continue;
        }
        total_len += 300 + strlen(entry->slug) + strlen(entry->title) +
                     strlen(entry->date);
    }
    
    list_html = arena_alloc(arena, total_len);
    if (!list_html) {
        return NULL;
    }
    
    list_html[0] = '\0';
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        if (strcmp(entry->lang, lang) != 0 || (entry->draft && !include_drafts)) {
            continue;
        }
        if (match_count >= start && match_count < end) {
            append_entry_link(list_html, total_len, &offset, entry, arena);
        }
        match_count++;
    }
    
    if (offset == 0) {
        return arena_strdup(arena, "<li>No posts on this page</li>\n");
    }
    
    return list_html;
}

/* Build pagination navigation HTML */
static char* build_pagination(arena_t* arena, const char* lang,
                              size_t page, size_t page_count)
{
    char buf[512];
    size_t offset = 0;
    const char* prefix;
    
    if (page_count <= 1) {
        return arena_strdup(arena, "");
    }
    
    prefix = (strcmp(lang, "en") == 0) ? "/en" : "";
    offset = snprintf(buf, sizeof(buf), "<nav class=\"pagination\">\n");
    
    if (page > 1) {
        if (page == 2) {
            offset += snprintf(buf + offset, sizeof(buf) - offset,
                               "<a href=\"%s\" class=\"newer\">← Newer Posts</a>\n",
                               prefix[0] ? "/en/" : "/");
        } else {
            offset += snprintf(buf + offset, sizeof(buf) - offset,
                               "<a href=\"%s/page/%zu/\" class=\"newer\">← Newer Posts</a>\n",
                               prefix, page - 1);
        }
    }
    
    if (page < page_count) {
        offset += snprintf(buf + offset, sizeof(buf) - offset,
                           "<a href=\"%s/page/%zu/\" class=\"older\">Older Posts →</a>\n",
                           prefix, page + 1);
    }
    
    offset += snprintf(buf + offset, sizeof(buf) - offset, "</nav>\n");
    
    return arena_strdup(arena, buf);
}

/* Convert date from YYYY-MM-DD to RFC 822 format for RSS
 * Returns pointer to static buffer
 */
static const char* rfc822_date(const char* date_str)
{
    static char buf[64];
    int year;
    int month;
    int day;
    struct tm tm_info;
    
    if (sscanf(date_str, "%d-%d-%d", &year, &month, &day) != 3) {
        return date_str;
    }
    
    memset(&tm_info, 0, sizeof(tm_info));
    tm_info.tm_year = year - 1900;
    tm_info.tm_mon = month - 1;
    tm_info.tm_mday = day;
    
    strftime(buf, sizeof(buf), "%a, %d %b %Y 00:00:00 GMT", &tm_info);
    return buf;
}

/* Escape XML special characters */
static void escape_xml(char* dst, const char* src, size_t size)
{
    size_t i;
    size_t j;
    
    if (!src || !dst || size == 0) {
        return;
    }
    
    j = 0;
    for (i = 0; src[i] && j < size - 1; i++) {
        switch (src[i]) {
        case '<':
            if (j + 4 < size) {
                strcpy(dst + j, "&lt;");
                j += 4;
            }
            break;
        case '>':
            if (j + 4 < size) {
                strcpy(dst + j, "&gt;");
                j += 4;
            }
            break;
        case '&':
            if (j + 5 < size) {
                strcpy(dst + j, "&amp;");
                j += 5;
            }
            break;
        default:
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

/* Strip HTML tags for RSS description */
static void strip_html(char* dst, const char* src, size_t size)
{
    size_t i;
    size_t j;
    int in_tag;
    
    if (!src || !dst || size == 0) {
        if (dst && size > 0) {
            dst[0] = '\0';
        }
        return;
    }
    
    j = 0;
    in_tag = 0;
    for (i = 0; src[i] && j < size - 1; i++) {
        if (src[i] == '<') {
            in_tag = 1;
        } else if (src[i] == '>') {
            in_tag = 0;
        } else if (!in_tag) {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

/* Generate RSS feed */
static int render_rss(cxo_context_t* ctx, arena_t* arena __attribute__((unused)),
                      const char* output_dir, const char* lang)
{
    char path[MAX_OUTPUT_PATH];
    FILE* fp;
    size_t i;
    const char* base_url;
    char escaped_title[256];
    char stripped_desc[1024];
    
    base_url = ctx->base_url ? ctx->base_url : "http://localhost";
    
    if (strcmp(lang, "en") == 0) {
        snprintf(path, sizeof(path), "%s/en/rss.xml", output_dir);
    } else {
        snprintf(path, sizeof(path), "%s/rss.xml", output_dir);
    }
    
    fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot write %s\n", path);
        return CXO_ERR_IO;
    }
    
    escape_xml(escaped_title, ctx->site_title, sizeof(escaped_title));
    escape_xml(stripped_desc, ctx->site_description ? ctx->site_description : "",
               sizeof(stripped_desc));
    
    fprintf(fp,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<rss version=\"2.0\" xmlns:atom=\"http://www.w3.org/2005/Atom\">\n"
            "<channel>\n"
            "<title>%s</title>\n"
            "<link>%s</link>\n"
            "<description>%s</description>\n"
            "<language>%s</language>\n"
            "<lastBuildDate>%s</lastBuildDate>\n",
            escaped_title, base_url, stripped_desc, lang, rfc822_date("2026-03-28"));
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        char item_title[256];
        char item_desc[1024];
        const char* subdir;
        
        if (strcmp(entry->lang, lang) != 0) {
            continue;
        }
        
        /* Skip draft posts in RSS */
        if (entry->draft) {
            continue;
        }
        
        subdir = get_output_subdir(entry->lang);
        
        escape_xml(item_title, entry->title, sizeof(item_title));
        strip_html(item_desc, entry->html_content, sizeof(item_desc));
        
        /* Truncate description */
        if (strlen(item_desc) > 500) {
            item_desc[500] = '\0';
            strcat(item_desc, "...");
        }
        
        fprintf(fp,
                "<item>\n"
                "<title>%s</title>\n"
                "<link>%s/%s/%s.html</link>\n"
                "<guid>%s/%s/%s.html</guid>\n"
                "<pubDate>%s</pubDate>\n"
                "<description><![CDATA[%s]]></description>\n"
                "</item>\n",
                item_title, base_url, subdir, entry->slug,
                base_url, subdir, entry->slug,
                rfc822_date(entry->date), item_desc);
    }
    
    fprintf(fp,
            "</channel>\n"
            "</rss>\n");
    
    if (ferror(fp)) {
        fclose(fp);
        fprintf(stderr, "Error: Write failed for %s\n", path);
        return CXO_ERR_IO;
    }
    
    fclose(fp);
    printf("Generated: %s\n", path);
    return CXO_OK;
}

/* Write sitemap XML header and home pages */
static void write_sitemap_header(FILE* fp, const char* base_url)
{
    fprintf(fp,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n");
    fprintf(fp,
            "<url>\n"
            "<loc>%s/</loc>\n"
            "<priority>1.0</priority>\n"
            "</url>\n", base_url);
    fprintf(fp,
            "<url>\n"
            "<loc>%s/en/</loc>\n"
            "<priority>1.0</priority>\n"
            "</url>\n", base_url);
}

/* Write a single sitemap entry */
static void write_sitemap_entry(FILE* fp, cxo_entry_t* entry,
                                const char* base_url)
{
    const char* subdir = get_output_subdir(entry->lang);
    fprintf(fp,
            "<url>\n"
            "<loc>%s/%s/%s.html</loc>\n"
            "<lastmod>%s</lastmod>\n"
            "<priority>0.8</priority>\n"
            "</url>\n",
            base_url, subdir, entry->slug, entry->date);
}

/* Generate sitemap */
static int render_sitemap(cxo_context_t* ctx, arena_t* arena __attribute__((unused)),
                          const char* output_dir)
{
    char path[MAX_OUTPUT_PATH];
    FILE* fp;
    size_t i;
    const char* base_url;
    
    base_url = ctx->base_url ? ctx->base_url : "http://localhost";
    snprintf(path, sizeof(path), "%s/sitemap.xml", output_dir);
    
    fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot write %s\n", path);
        return CXO_ERR_IO;
    }
    
    write_sitemap_header(fp, base_url);
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        if (entry->draft) {
            continue;
        }
        write_sitemap_entry(fp, entry, base_url);
    }
    
    /* Add paginated index pages to sitemap */
    {
        size_t zh_count = count_matching_entries(ctx, "zh", 0);
        size_t en_count = count_matching_entries(ctx, "en", 0);
        size_t zh_pages = ctx->posts_per_page > 0 ?
            (zh_count + ctx->posts_per_page - 1) / ctx->posts_per_page : 1;
        size_t en_pages = ctx->posts_per_page > 0 ?
            (en_count + ctx->posts_per_page - 1) / ctx->posts_per_page : 1;
        size_t p;
        
        for (p = 2; p <= zh_pages; p++) {
            fprintf(fp,
                    "<url>\n"
                    "<loc>%s/page/%zu/</loc>\n"
                    "<priority>0.6</priority>\n"
                    "</url>\n",
                    base_url, p);
        }
        
        for (p = 2; p <= en_pages; p++) {
            fprintf(fp,
                    "<url>\n"
                    "<loc>%s/en/page/%zu/</loc>\n"
                    "<priority>0.6</priority>\n"
                    "</url>\n",
                    base_url, p);
        }
    }
    
    /* Add archive pages to sitemap */
    {
        char years[MAX_ARCHIVE_PERIODS][5];
        char months[MAX_ARCHIVE_PERIODS][8];
        size_t year_count = 0;
        size_t month_count = 0;
        size_t j;
        int found;
        
        for (i = 0; i < ctx->count; i++) {
            cxo_entry_t* entry = ctx->entries[i];
            char year[5];
            char month[8];
            
            if (entry->draft || strlen(entry->date) < 4) {
                continue;
            }
            
            memcpy(year, entry->date, 4);
            year[4] = '\0';
            snprintf(month, sizeof(month), "%s-%c%c", year,
                     entry->date[5], entry->date[6]);
            
            found = 0;
            for (j = 0; j < year_count; j++) {
                if (strcmp(years[j], year) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found && year_count < MAX_ARCHIVE_PERIODS) {
                strcpy(years[year_count++], year);
            }
            
            found = 0;
            for (j = 0; j < month_count; j++) {
                if (strcmp(months[j], month) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found && month_count < MAX_ARCHIVE_PERIODS) {
                strcpy(months[month_count++], month);
            }
        }
        
        for (j = 0; j < year_count; j++) {
            fprintf(fp,
                    "<url>\n"
                    "<loc>%s/%s/</loc>\n"
                    "<priority>0.5</priority>\n"
                    "</url>\n",
                    base_url, years[j]);
            fprintf(fp,
                    "<url>\n"
                    "<loc>%s/en/%s/</loc>\n"
                    "<priority>0.5</priority>\n"
                    "</url>\n",
                    base_url, years[j]);
        }
        
        for (j = 0; j < month_count; j++) {
            fprintf(fp,
                    "<url>\n"
                    "<loc>%s/%c%c%c%c/%c%c/</loc>\n"
                    "<priority>0.5</priority>\n"
                    "</url>\n",
                    base_url,
                    months[j][0], months[j][1], months[j][2], months[j][3],
                    months[j][5], months[j][6]);
            fprintf(fp,
                    "<url>\n"
                    "<loc>%s/en/%c%c%c%c/%c%c/</loc>\n"
                    "<priority>0.5</priority>\n"
                    "</url>\n",
                    base_url,
                    months[j][0], months[j][1], months[j][2], months[j][3],
                    months[j][5], months[j][6]);
        }
    }
    
    fprintf(fp, "</urlset>\n");
    
    if (ferror(fp)) {
        fclose(fp);
        fprintf(stderr, "Error: Write failed for %s\n", path);
        return CXO_ERR_IO;
    }
    
    fclose(fp);
    printf("Generated: %s\n", path);
    return CXO_OK;
}

#define MAX_UNIQUE_TAGS 256

/* Collect unique tags across all entries */
static size_t collect_unique_tags(cxo_context_t* ctx, char** tags_out)
{
    size_t i, j, k;
    size_t count = 0;
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        for (j = 0; j < entry->tag_count; j++) {
            char* tag = entry->tags[j];
            int found = 0;
            for (k = 0; k < count; k++) {
                if (strcmp(tags_out[k], tag) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found && count < MAX_UNIQUE_TAGS) {
                tags_out[count++] = tag;
            }
        }
    }
    return count;
}

/* Build entry list for a specific tag and language */
static char* build_tag_entry_list(cxo_context_t* ctx, arena_t* arena,
                                  const char* tag, const char* lang)
{
    size_t i, j;
    size_t total_len = 256;
    char* list_html;
    size_t offset;
    int found = 0;
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        int has_tag = 0;
        
        if (strcmp(entry->lang, lang) != 0 || entry->draft) {
            continue;
        }
        
        for (j = 0; j < entry->tag_count; j++) {
            if (strcmp(entry->tags[j], tag) == 0) {
                has_tag = 1;
                break;
            }
        }
        
        if (has_tag) {
            found = 1;
            total_len += 100 + strlen(entry->slug) + strlen(entry->title) +
                         strlen(entry->date);
        }
    }
    
    if (!found) {
        return NULL;
    }
    
    list_html = arena_alloc(arena, total_len);
    if (!list_html) {
        return NULL;
    }
    
    list_html[0] = '\0';
    offset = 0;
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        int has_tag = 0;
        
        if (strcmp(entry->lang, lang) != 0 || entry->draft) {
            continue;
        }
        
        for (j = 0; j < entry->tag_count; j++) {
            if (strcmp(entry->tags[j], tag) == 0) {
                has_tag = 1;
                break;
            }
        }
        
        if (has_tag) {
            const char* subdir = get_output_subdir(entry->lang);
            int written = snprintf(list_html + offset, total_len - offset,
                                   "<li><a href=\"/%s/%s.html\">%s</a> <span class=\"date\">%s</span></li>\n",
                                   subdir, entry->slug, entry->title,
                                   entry->date);
            if (written > 0) {
                offset += written;
            }
        }
    }
    
    return list_html;
}

/* Render a single tag page */
static int render_tag_page(cxo_context_t* ctx, arena_t* arena,
                           const char* output_dir, const char* lang,
                           const char* tag, const char* tmpl)
{
    char path[MAX_OUTPUT_PATH];
    char* list_html;
    char* html;
    const char* subdir;
    
    list_html = build_tag_entry_list(ctx, arena, tag, lang);
    if (!list_html) {
        return CXO_OK;
    }
    
    subdir = (strcmp(lang, "en") == 0) ? "en/tags" : "tags";
    snprintf(path, sizeof(path), "%s/%s", output_dir, subdir);
    if (ensure_dir(path) != CXO_OK) {
        return CXO_ERR_IO;
    }
    
    html = replace_var(arena, tmpl, "lang", lang);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "site_title", ctx->site_title);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "site_description", ctx->site_description);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "tag_name", tag);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "entry_list", list_html);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "hotreload",
                       hotreload_enabled() ? hotreload_script : "");
    if (!html) {
        html = "";
    }
    
    snprintf(path, sizeof(path), "%s/%s/%s.html", output_dir, subdir, tag);
    return write_html(path, html);
}

/* Build entry list for archives matching year/month and language */
static char* build_archive_entry_list(cxo_context_t* ctx, arena_t* arena,
                                      const char* lang, const char* year,
                                      const char* month)
{
    size_t i;
    size_t total_len = 256;
    char* list_html;
    size_t offset = 0;
    int found = 0;
    int include_drafts = show_drafts();
    size_t year_len = strlen(year);
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        if (strcmp(entry->lang, lang) != 0 || (entry->draft && !include_drafts)) {
            continue;
        }
        if (strncmp(entry->date, year, year_len) == 0) {
            if (!month || strncmp(entry->date + year_len + 1, month, 2) == 0) {
                found = 1;
                total_len += 100 + strlen(entry->slug) + strlen(entry->title) +
                             strlen(entry->date);
            }
        }
    }
    
    if (!found) {
        return NULL;
    }
    
    list_html = arena_alloc(arena, total_len);
    if (!list_html) {
        return NULL;
    }
    
    list_html[0] = '\0';
    
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        if (strcmp(entry->lang, lang) != 0 || (entry->draft && !include_drafts)) {
            continue;
        }
        if (strncmp(entry->date, year, year_len) == 0) {
            if (!month || strncmp(entry->date + year_len + 1, month, 2) == 0) {
                const char* subdir = get_output_subdir(entry->lang);
                int written = snprintf(list_html + offset, total_len - offset,
                                       "<li><a href=\"/%s/%s.html\">%s</a> <span class=\"date\">%s</span></li>\n",
                                       subdir, entry->slug, entry->title,
                                       entry->date);
                if (written > 0) {
                    offset += written;
                }
            }
        }
    }
    
    return list_html;
}

/* Render a single archive page */
static int render_archive_page(cxo_context_t* ctx, arena_t* arena,
                               const char* output_dir, const char* lang,
                               const char* year, const char* month,
                               const char* tmpl)
{
    char path[MAX_OUTPUT_PATH];
    char* list_html;
    char* html;
    const char* prefix;
    char title[64];
    
    list_html = build_archive_entry_list(ctx, arena, lang, year, month);
    if (!list_html) {
        return CXO_OK;
    }
    
    prefix = (strcmp(lang, "en") == 0) ? "en/" : "";
    
    if (month) {
        char dir_path[MAX_OUTPUT_PATH];
        snprintf(dir_path, sizeof(dir_path), "%s/%s%s/%s",
                 output_dir, prefix, year, month);
        snprintf(path, sizeof(path), "%s/index.html", dir_path);
        snprintf(title, sizeof(title), "%s-%s", year, month);
        if (ensure_dir(dir_path) != CXO_OK) {
            return CXO_ERR_IO;
        }
    } else {
        char dir_path[MAX_OUTPUT_PATH];
        snprintf(dir_path, sizeof(dir_path), "%s/%s%s",
                 output_dir, prefix, year);
        snprintf(path, sizeof(path), "%s/index.html", dir_path);
        snprintf(title, sizeof(title), "%s", year);
        if (ensure_dir(dir_path) != CXO_OK) {
            return CXO_ERR_IO;
        }
    }
    
    html = replace_var(arena, tmpl, "lang", lang);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "site_title", ctx->site_title);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "site_description", ctx->site_description);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "archive_title", title);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "tag_name", title);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "entry_list", list_html);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "hotreload",
                       hotreload_enabled() ? hotreload_script : "");
    if (!html) {
        html = "";
    }
    
    return write_html(path, html);
}

/* Render all archive pages for a language */
static int render_archive_pages(cxo_context_t* ctx, arena_t* arena,
                                const char* output_dir, const char* lang,
                                const char* tmpl)
{
    size_t i;
    char years[MAX_ARCHIVE_PERIODS][5];
    char months[MAX_ARCHIVE_PERIODS][8];
    size_t year_count = 0;
    size_t month_count = 0;
    size_t y, m;
    int include_drafts = show_drafts();
    
    /* Collect unique years and year-months */
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        char year[5];
        char month[8];
        int found;
        size_t j;
        
        if (strcmp(entry->lang, lang) != 0 || (entry->draft && !include_drafts)) {
            continue;
        }
        
        if (strlen(entry->date) < 4) {
            continue;
        }
        
        memcpy(year, entry->date, 4);
        year[4] = '\0';
        snprintf(month, sizeof(month), "%s-%c%c", year,
                 entry->date[5], entry->date[6]);
        
        /* Add unique year */
        found = 0;
        for (j = 0; j < year_count; j++) {
            if (strcmp(years[j], year) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && year_count < MAX_ARCHIVE_PERIODS) {
            strcpy(years[year_count++], year);
        }
        
        /* Add unique year-month */
        found = 0;
        for (j = 0; j < month_count; j++) {
            if (strcmp(months[j], month) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && month_count < MAX_ARCHIVE_PERIODS) {
            strcpy(months[month_count++], month);
        }
    }
    
    /* Render year archives */
    for (y = 0; y < year_count; y++) {
        int rc = render_archive_page(ctx, arena, output_dir, lang,
                                      years[y], NULL, tmpl);
        if (CXO_IS_ERR(rc)) {
            return rc;
        }
    }
    
    /* Render month archives */
    for (m = 0; m < month_count; m++) {
        char yr[5];
        char mo[3];
        int rc;
        memcpy(yr, months[m], 4);
        yr[4] = '\0';
        mo[0] = months[m][5];
        mo[1] = months[m][6];
        mo[2] = '\0';
        rc = render_archive_page(ctx, arena, output_dir, lang, yr, mo, tmpl);
        if (CXO_IS_ERR(rc)) {
            return rc;
        }
    }
    
    return CXO_OK;
}

/* Render a single index page */
static int render_index_page(cxo_context_t* ctx, arena_t* arena,
                              const char* output_dir, const char* lang,
                              const char* tmpl, size_t page, size_t page_count,
                              size_t per_page)
{
    char path[MAX_OUTPUT_PATH];
    char dir_path[MAX_OUTPUT_PATH];
    char* entry_list;
    char* html;
    char* pagination;
    
    if (page == 1) {
        if (strcmp(lang, "en") == 0) {
            snprintf(path, sizeof(path), "%s/en/index.html", output_dir);
        } else {
            snprintf(path, sizeof(path), "%s/index.html", output_dir);
        }
    } else {
        if (strcmp(lang, "en") == 0) {
            snprintf(path, sizeof(path), "%s/en/page/%zu/index.html",
                     output_dir, page);
        } else {
            snprintf(path, sizeof(path), "%s/page/%zu/index.html",
                     output_dir, page);
        }
    }
    
    /* Ensure directory exists for paginated pages */
    strncpy(dir_path, path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    {
        char* last_slash = strrchr(dir_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (ensure_dir(dir_path) != CXO_OK) {
                fprintf(stderr, "Error: Cannot create directory %s\n", dir_path);
                return CXO_ERR_IO;
            }
        }
    }
    
    if (per_page > 0 && page_count > 1) {
        entry_list = build_paginated_entry_list(ctx, arena, lang, page, per_page);
    } else {
        entry_list = build_entry_list(ctx, arena, lang);
    }
    if (!entry_list) {
        entry_list = "";
    }
    
    pagination = build_pagination(arena, lang, page, page_count);
    if (!pagination) {
        pagination = "";
    }
    
    html = replace_var(arena, tmpl, "lang", lang);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "site_title", ctx->site_title);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "site_description", ctx->site_description);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "entry_list", entry_list);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "pagination", pagination);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "hotreload",
                       hotreload_enabled() ? hotreload_script : "");
    if (!html) {
        html = "";
    }
    
    return write_html(path, html);
}

/* Generate index page(s) with pagination */
static int render_index(cxo_context_t* ctx, arena_t* arena,
                        const char* output_dir, const char* lang,
                        const char* tmpl)
{
    size_t total_entries;
    size_t per_page = ctx->posts_per_page;
    size_t page_count;
    size_t page;
    int rc;
    
    total_entries = count_matching_entries(ctx, lang, show_drafts());
    
    if (per_page == 0 || total_entries <= per_page) {
        return render_index_page(ctx, arena, output_dir, lang, tmpl,
                                  1, 1, 0);
    }
    
    page_count = (total_entries + per_page - 1) / per_page;
    
    for (page = 1; page <= page_count; page++) {
        rc = render_index_page(ctx, arena, output_dir, lang, tmpl,
                                page, page_count, per_page);
        if (CXO_IS_ERR(rc)) {
            return rc;
        }
    }
    
    return CXO_OK;
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
    rc = copy_theme_assets(ctx->theme_path, output_dir);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Warning: Failed to copy theme assets\n");
    }
    
    /* Sort entries by date descending and assign prev/next navigation */
    sort_entries(ctx);
    assign_prev_next(ctx);
    
    /* Render individual entries */
    success = 0;
    for (i = 0; i < ctx->count; i++) {
        rc = render_entry(ctx->entries[i], ctx, arena, output_dir, tmpl);
        if (!CXO_IS_ERR(rc)) {
            success++;
        }
    }
    
    /* Generate index pages */
    {
        char* index_tmpl = load_index_template(arena, ctx->theme_path);
        rc = render_index(ctx, arena, output_dir, "zh", index_tmpl);
        if (CXO_IS_ERR(rc)) {
            return rc;
        }
        rc = render_index(ctx, arena, output_dir, "en", index_tmpl);
        if (CXO_IS_ERR(rc)) {
            return rc;
        }
    }
    
    /* Generate tag pages */
    {
        char* tag_tmpl = load_tag_template(arena, ctx->theme_path);
        char* unique_tags[MAX_UNIQUE_TAGS];
        size_t tag_count;
        size_t t;
        
        tag_count = collect_unique_tags(ctx, unique_tags);
        for (t = 0; t < tag_count; t++) {
            rc = render_tag_page(ctx, arena, output_dir, "zh", unique_tags[t],
                                 tag_tmpl);
            if (CXO_IS_ERR(rc)) {
                return rc;
            }
            rc = render_tag_page(ctx, arena, output_dir, "en", unique_tags[t],
                                 tag_tmpl);
            if (CXO_IS_ERR(rc)) {
                return rc;
            }
        }
    }
    
    /* Generate archive pages */
    {
        char* archive_tmpl = load_archive_template(arena, ctx->theme_path);
        rc = render_archive_pages(ctx, arena, output_dir, "zh", archive_tmpl);
        if (CXO_IS_ERR(rc)) {
            return rc;
        }
        rc = render_archive_pages(ctx, arena, output_dir, "en", archive_tmpl);
        if (CXO_IS_ERR(rc)) {
            return rc;
        }
    }
    
    /* Generate RSS feeds */
    rc = render_rss(ctx, arena, output_dir, "zh");
    if (CXO_IS_ERR(rc)) {
        return rc;
    }
    rc = render_rss(ctx, arena, output_dir, "en");
    if (CXO_IS_ERR(rc)) {
        return rc;
    }
    
    /* Generate sitemap */
    rc = render_sitemap(ctx, arena, output_dir);
    if (CXO_IS_ERR(rc)) {
        return rc;
    }
    
    /* Copy static assets */
    rc = copy_static_files(output_dir);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Warning: Failed to copy static files\n");
    }
    
    printf("Rendered %d/%zu entries\n", success, ctx->count);
    
    return (success == (int)ctx->count) ? CXO_OK : CXO_ERR_RENDER;
}
