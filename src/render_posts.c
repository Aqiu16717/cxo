/*
 * render_posts.c - Post page rendering, SEO meta tags and prev/next navigation
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "renderer_internal.h"

/* Build language switch */
static char* build_lang_switch(arena_t* arena, const cxo_entry_t* entry)
{
    char buf[256];
    const cxo_lang_t* peer_lang;
    char* peer_url;

    if (!entry->peer) {
        return arena_strdup(arena, "");
    }

    peer_lang = cxo_lang_find(entry->peer->lang);
    peer_url = cxo_entry_url(arena, entry->peer);
    snprintf(buf, sizeof(buf), "<a href=\"%s\">%s</a>",
             peer_url ? peer_url : "",
             peer_lang ? peer_lang->label : entry->peer->lang);
    return arena_strdup(arena, buf);
}

/* Build prev/next navigation link */
static char* build_nav_link(arena_t* arena, cxo_entry_t* entry,
                            const char* rel_class)
{
    char buf[512];
    char* url;

    if (!entry) {
        return arena_strdup(arena, "");
    }

    url = cxo_entry_url(arena, entry);
    snprintf(buf, sizeof(buf),
             "<a href=\"%s\" class=\"%s\">%s</a>",
             url ? url : "", rel_class, escape_attr(arena, entry->title));
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
void sort_entries(cxo_context_t* ctx)
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
void assign_prev_next(cxo_context_t* ctx)
{
    size_t i;
    cxo_entry_t* last[CXO_MAX_LANGS];

    for (i = 0; i < CXO_MAX_LANGS; i++) {
        last[i] = NULL;
    }

    /* First pass (newest to oldest): assign next (newer) pointers */
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        size_t li = cxo_lang_index(entry->lang);

        entry->next = last[li];
        entry->prev = NULL;
        last[li] = entry;
    }

    /* Second pass (oldest to newest): assign prev (older) pointers */
    for (i = 0; i < CXO_MAX_LANGS; i++) {
        last[i] = NULL;
    }
    for (i = ctx->count; i > 0; i--) {
        cxo_entry_t* entry = ctx->entries[i - 1];
        size_t li = cxo_lang_index(entry->lang);

        entry->prev = last[li];
        last[li] = entry;
    }
}

/* Build SEO meta tags (shared core for post and site pages) */
static char* build_meta_tags(arena_t* arena, const char* title,
                             const char* desc, const char* url,
                             const char* og_type, const char* locale)
{
    char locale_line[80];
    char* etitle;
    char* edesc;
    char* buf;
    size_t size;
    int n;

    etitle = escape_attr(arena, title);
    edesc = escape_attr(arena, desc);
    if (!etitle || !edesc) {
        return arena_strdup(arena, "");
    }

    if (locale) {
        snprintf(locale_line, sizeof(locale_line),
                 "<meta property=\"og:locale\" content=\"%s\">\n", locale);
    } else {
        locale_line[0] = '\0';
    }

    /* title appears 2x, desc 3x; sized so snprintf can never truncate */
    size = strlen(etitle) * 2 + strlen(edesc) * 3 + strlen(url) + 512;
    buf = arena_alloc(arena, size);
    if (!buf) {
        return arena_strdup(arena, "");
    }

    n = snprintf(buf, size,
                 "<meta name=\"description\" content=\"%s\">\n"
                 "<meta property=\"og:title\" content=\"%s\">\n"
                 "<meta property=\"og:description\" content=\"%s\">\n"
                 "<meta property=\"og:url\" content=\"%s\">\n"
                 "<meta property=\"og:type\" content=\"%s\">\n"
                 "%s"
                 "<meta name=\"twitter:card\" content=\"summary\">\n"
                 "<meta name=\"twitter:title\" content=\"%s\">\n"
                 "<meta name=\"twitter:description\" content=\"%s\">\n",
                 edesc, etitle, edesc, url, og_type, locale_line,
                 etitle, edesc);
    if (n < 0 || (size_t)n >= size) {
        return arena_strdup(arena, "");
    }
    return buf;
}

/* Build SEO meta tags for a post */
static char* build_post_meta_tags(cxo_entry_t* entry,
                                  const cxo_context_t* ctx,
                                  arena_t* arena)
{
    char url[1024];
    const char* base;
    char* entry_url;

    base = ctx->base_url ? ctx->base_url : "";
    entry_url = cxo_entry_url(arena, entry);
    snprintf(url, sizeof(url), "%s%s", base, entry_url ? entry_url : "");

    return build_meta_tags(arena, entry->title,
                           entry->description ? entry->description : "",
                           url, "article", NULL);
}

/* Build SEO meta tags for site pages (index, tag, archive) */
char* build_site_meta_tags(const cxo_context_t* ctx,
                                  arena_t* arena, const char* lang,
                                  const char* page_url)
{
    char url[1024];
    const char* base;
    const char* title;
    const char* desc;
    const char* locale;

    base = ctx->base_url ? ctx->base_url : "";
    title = ctx->site_title ? ctx->site_title : "";
    desc = ctx->site_description ? ctx->site_description : "";
    {
        const cxo_lang_t* l = cxo_lang_find(lang);
        locale = l ? l->locale : CXO_LANGS[0].locale;
    }

    snprintf(url, sizeof(url), "%s%s", base, page_url);
    return build_meta_tags(arena, title, desc, url, "website", locale);
}

/* Build tag links HTML for a post */
static char* build_tag_links(cxo_entry_t* entry, arena_t* arena)
{
    size_t i;
    size_t total_len;
    char* buf;
    size_t offset;
    char tag_prefix[64];

    if (entry->tag_count == 0) {
        return arena_strdup(arena, "");
    }

    {
        const cxo_lang_t* l = cxo_lang_find(entry->lang);
        if (l && l->prefix[0]) {
            snprintf(tag_prefix, sizeof(tag_prefix), "/%s/tags/", l->prefix);
        } else {
            snprintf(tag_prefix, sizeof(tag_prefix), "/tags/");
        }
    }
    
    total_len = 32;
    for (i = 0; i < entry->tag_count; i++) {
        total_len += strlen(tag_prefix) + strlen(entry->tags[i]) * 12 + 32;
    }
    
    buf = arena_alloc(arena, total_len);
    if (!buf) {
        return NULL;
    }
    
    offset = 0;
    
    for (i = 0; i < entry->tag_count; i++) {
        char* esc_tag = escape_attr(arena, entry->tags[i]);
        if (i > 0) {
            offset += snprintf(buf + offset, total_len - offset, " ");
        }
        offset += snprintf(buf + offset, total_len - offset,
                           "<a href=\"%s%s.html\">%s</a>",
                           tag_prefix, esc_tag, esc_tag);
    }
    
    return buf;
}

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
    
    html = replace_var(arena, tmpl, "title", escape_attr(arena, entry->title));
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "date", escape_attr(arena, entry->date));
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
    html = replace_var(arena, html, "description",
                       escape_attr(arena, description));
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
    html = replace_var(arena, html, "site_title",
                       escape_attr(arena, ctx->site_title));
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "site_description",
                       escape_attr(arena, ctx->site_description));
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "toc", entry->toc ? entry->toc : "");
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "meta_tags",
                       build_post_meta_tags(entry, ctx, arena));
    if (!html) {
        return NULL;
    }
    html = replace_var(arena, html, "hotreload",
                       hotreload_enabled() ? hotreload_script : "");
    
    return html;
}

/* Write HTML file */
int write_html(const char* path, const char* html)
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
int render_entry(cxo_entry_t* entry, cxo_context_t* ctx,
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

