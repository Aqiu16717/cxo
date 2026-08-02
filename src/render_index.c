/*
 * render_index.c - Index page rendering with pagination
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "renderer_internal.h"

#define EXCERPT_MAX_LEN 150

/* Count entries matching language and draft filter */
size_t count_matching_entries(cxo_context_t* ctx, const char* lang,
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
        total_len += 1200 + strlen(entry->slug) + strlen(entry->title) * 6 +
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

/* Append a single entry link to the list buffer */
static void append_entry_link(char* buf, size_t total_len, size_t* offset,
                              cxo_entry_t* entry, arena_t* arena)
{
    char* entry_url;
    char* excerpt;
    int written;

    excerpt = build_excerpt(entry, arena);
    if (!excerpt) {
        excerpt = "";
    }
    entry_url = cxo_entry_url(arena, entry);

    written = snprintf(buf + *offset, total_len - *offset,
                       "<li><a href=\"%s\">%s</a> <span class=\"date\">%s</span><div class=\"excerpt\">%s</div></li>\n",
                       entry_url ? entry_url : "",
                       escape_attr(arena, entry->title), entry->date,
                       escape_attr(arena, excerpt));
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
        total_len += 1200 + strlen(entry->slug) + strlen(entry->title) * 6 +
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
    char prefix[40];
    char home[44];

    if (page_count <= 1) {
        return arena_strdup(arena, "");
    }

    {
        const cxo_lang_t* l = cxo_lang_find(lang);
        if (l && l->prefix[0]) {
            snprintf(prefix, sizeof(prefix), "/%s", l->prefix);
        } else {
            prefix[0] = '\0';
        }
    }
    snprintf(home, sizeof(home), "%s/", prefix);
    offset = snprintf(buf, sizeof(buf), "<nav class=\"pagination\">\n");

    if (page > 1) {
        if (page == 2) {
            offset += snprintf(buf + offset, sizeof(buf) - offset,
                               "<a href=\"%s\" class=\"newer\">← Newer Posts</a>\n",
                               home);
        } else {
            offset += snprintf(buf + offset, sizeof(buf) - offset,
                               "<a href=\"%s/page/%lu/\" class=\"newer\">← Newer Posts</a>\n",
                               prefix, (unsigned long)(page - 1));
        }
    }
    
    if (page < page_count) {
        offset += snprintf(buf + offset, sizeof(buf) - offset,
                           "<a href=\"%s/page/%lu/\" class=\"older\">Older Posts →</a>\n",
                           prefix, (unsigned long)(page + 1));
    }
    
    offset += snprintf(buf + offset, sizeof(buf) - offset, "</nav>\n");
    
    return arena_strdup(arena, buf);
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
    const cxo_lang_t* l = cxo_lang_find(lang);
    const char* pfx = (l && l->prefix[0]) ? l->prefix : NULL;

    if (page == 1) {
        if (pfx) {
            snprintf(path, sizeof(path), "%s/%s/index.html", output_dir, pfx);
        } else {
            snprintf(path, sizeof(path), "%s/index.html", output_dir);
        }
    } else {
        if (pfx) {
            snprintf(path, sizeof(path), "%s/%s/page/%lu/index.html",
                     output_dir, pfx, (unsigned long)page);
        } else {
            snprintf(path, sizeof(path), "%s/page/%lu/index.html",
                     output_dir, (unsigned long)page);
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
    
    {
        char page_url[128];
        if (page == 1) {
            if (pfx) {
                snprintf(page_url, sizeof(page_url), "/%s/", pfx);
            } else {
                snprintf(page_url, sizeof(page_url), "/");
            }
        } else {
            if (pfx) {
                snprintf(page_url, sizeof(page_url), "/%s/page/%lu/",
                         pfx, (unsigned long)page);
            } else {
                snprintf(page_url, sizeof(page_url), "/page/%lu/",
                         (unsigned long)page);
            }
        }
        html = replace_var(arena, tmpl, "meta_tags",
                           build_site_meta_tags(ctx, arena, lang, page_url));
    }
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "lang", lang);
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "site_title",
                       escape_attr(arena, ctx->site_title));
    if (!html) {
        html = "";
    }
    html = replace_var(arena, html, "site_description",
                       escape_attr(arena, ctx->site_description));
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
int render_index(cxo_context_t* ctx, arena_t* arena,
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

