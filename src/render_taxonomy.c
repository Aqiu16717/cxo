/*
 * render_taxonomy.c - Tag and archive page rendering
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <string.h>
#include "renderer_internal.h"

/* Collect unique tags across all entries */
size_t collect_unique_tags(cxo_context_t* ctx, char** tags_out)
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
            char* entry_url = cxo_entry_url(arena, entry);
            int written = snprintf(list_html + offset, total_len - offset,
                                   "<li><a href=\"%s\">%s</a> <span class=\"date\">%s</span></li>\n",
                                   entry_url ? entry_url : "",
                                   escape_attr(arena, entry->title),
                                   entry->date);
            if (written > 0) {
                offset += written;
            }
        }
    }
    
    return list_html;
}

/* Render a single tag page */
int render_tag_page(cxo_context_t* ctx, arena_t* arena,
                           const char* output_dir, const char* lang,
                           const char* tag, const char* tmpl)
{
    char path[MAX_OUTPUT_PATH];
    char* list_html;
    char* html;
    char subdir[64];

    list_html = build_tag_entry_list(ctx, arena, tag, lang);
    if (!list_html) {
        return CXO_OK;
    }

    {
        const cxo_lang_t* l = cxo_lang_find(lang);
        if (l && l->prefix[0]) {
            snprintf(subdir, sizeof(subdir), "%s/tags", l->prefix);
        } else {
            snprintf(subdir, sizeof(subdir), "tags");
        }
    }
    snprintf(path, sizeof(path), "%s/%s", output_dir, subdir);
    if (ensure_dir(path) != CXO_OK) {
        return CXO_ERR_IO;
    }
    
    {
        size_t url_size = strlen(subdir) + strlen(tag) + 8;
        char* page_url = arena_alloc(arena, url_size);
        if (!page_url) {
            return CXO_ERR_NOMEM;
        }
        snprintf(page_url, url_size, "/%s/%s.html", subdir, tag);
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
    html = replace_var(arena, html, "tag_name", escape_attr(arena, tag));
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
                char* entry_url = cxo_entry_url(arena, entry);
                int written = snprintf(list_html + offset, total_len - offset,
                                       "<li><a href=\"%s\">%s</a> <span class=\"date\">%s</span></li>\n",
                                       entry_url ? entry_url : "",
                                       escape_attr(arena, entry->title),
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
    char prefix[40];
    char title[64];

    list_html = build_archive_entry_list(ctx, arena, lang, year, month);
    if (!list_html) {
        return CXO_OK;
    }

    {
        const cxo_lang_t* l = cxo_lang_find(lang);
        if (l && l->prefix[0]) {
            snprintf(prefix, sizeof(prefix), "%s/", l->prefix);
        } else {
            prefix[0] = '\0';
        }
    }
    
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
    
    {
        char page_url[128];
        if (month) {
            snprintf(page_url, sizeof(page_url), "/%s%s/%s/", prefix, year, month);
        } else {
            snprintf(page_url, sizeof(page_url), "/%s%s/", prefix, year);
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
int render_archive_pages(cxo_context_t* ctx, arena_t* arena,
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

