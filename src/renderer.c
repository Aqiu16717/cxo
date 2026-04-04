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
#include "../include/cxo.h"

#define MAX_OUTPUT_PATH 4096
#define MAX_TEMPLATE_SIZE (256 * 1024)

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
    "<div class=\"content\">{{content}}</div>\n"
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

/* Generate HTML */
static char* generate_html(cxo_entry_t* entry, const cxo_context_t* ctx,
                           arena_t* arena, const char* tmpl)
{
    char* html;
    char* lang_switch;
    
    lang_switch = build_lang_switch(arena, entry);
    
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
    html = replace_var(arena, html, "site_title", ctx->site_title);
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "site_description", ctx->site_description);
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
    
    /* Count matching entries (excluding drafts unless in draft mode) */
    count = 0;
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        if (strcmp(entry->lang, lang) == 0 && (!entry->draft || include_drafts)) {
            count++;
        }
    }
    
    if (count == 0) {
        return arena_strdup(arena, "<li>No posts yet</li>\n");
    }
    
    /* Calculate total length needed */
    total_len = 256;  /* Base buffer */
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        if (strcmp(entry->lang, lang) != 0 || (entry->draft && !include_drafts)) {
            continue;
        }
        /* <li><a href="/posts/xxx.html">Title</a> <span class="date">date</span></li>\n */
        total_len += 100 + strlen(entry->slug) + strlen(entry->title) +
                     strlen(entry->date);
    }
    
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
        const char* subdir = get_output_subdir(entry->lang);
        int written = snprintf(list_html + offset, total_len - offset,
                               "<li><a href=\"/%s/%s.html\">%s</a> <span class=\"date\">%s</span></li>\n",
                               subdir, entry->slug, entry->title, entry->date);
        if (written > 0) {
            offset += written;
        }
    }
    
    return list_html;
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
    
    fclose(fp);
    printf("Generated: %s\n", path);
    return CXO_OK;
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
    
    fprintf(fp,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n");
    
    /* Home pages */
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
    
    /* Entries */
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        const char* subdir;
        
        if (entry->draft) {
            continue;
        }
        
        subdir = get_output_subdir(entry->lang);
        fprintf(fp,
                "<url>\n"
                "<loc>%s/%s/%s.html</loc>\n"
                "<lastmod>%s</lastmod>\n"
                "<priority>0.8</priority>\n"
                "</url>\n",
                base_url, subdir, entry->slug, entry->date);
    }
    
    fprintf(fp, "</urlset>\n");
    fclose(fp);
    printf("Generated: %s\n", path);
    return CXO_OK;
}

/* Generate index page */
static int render_index(cxo_context_t* ctx, arena_t* arena,
                        const char* output_dir, const char* lang,
                        const char* tmpl)
{
    char path[MAX_OUTPUT_PATH];
    FILE* fp;
    char* entry_list;
    char* html;
    
    if (strcmp(lang, "en") == 0) {
        snprintf(path, sizeof(path), "%s/en/index.html", output_dir);
    } else {
        snprintf(path, sizeof(path), "%s/index.html", output_dir);
    }
    
    entry_list = build_entry_list(ctx, arena, lang);
    if (!entry_list) {
        entry_list = "";
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
    html = replace_var(arena, html, "hotreload",
                       hotreload_enabled() ? hotreload_script : "");
    if (!html) {
        html = "";
    }
    
    fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Error: Cannot write %s\n", path);
        return CXO_ERR_IO;
    }
    
    fprintf(fp, "%s", html);
    fclose(fp);
    printf("Generated: %s\n", path);
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
        render_index(ctx, arena, output_dir, "zh", index_tmpl);
        render_index(ctx, arena, output_dir, "en", index_tmpl);
    }
    
    /* Generate RSS feeds */
    render_rss(ctx, arena, output_dir, "zh");
    render_rss(ctx, arena, output_dir, "en");
    
    /* Generate sitemap */
    render_sitemap(ctx, arena, output_dir);
    
    printf("Rendered %d/%zu entries\n", success, ctx->count);
    
    return (success == (int)ctx->count) ? CXO_OK : CXO_ERR_RENDER;
}
