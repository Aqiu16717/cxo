/*
 * render_feeds.c - RSS feed and sitemap generation
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "renderer_internal.h"

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
int render_rss(cxo_context_t* ctx, arena_t* arena,
                      const char* output_dir, const char* lang)
{
    char path[MAX_OUTPUT_PATH];
    FILE* fp;
    size_t i;
    const char* base_url;
    char escaped_title[256];
    char stripped_desc[1024];
    
    base_url = ctx->base_url ? ctx->base_url : "http://localhost";

    {
        const cxo_lang_t* l = cxo_lang_find(lang);
        if (l && l->prefix[0]) {
            snprintf(path, sizeof(path), "%s/%s/rss.xml", output_dir, l->prefix);
        } else {
            snprintf(path, sizeof(path), "%s/rss.xml", output_dir);
        }
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
        char* entry_url;

        if (strcmp(entry->lang, lang) != 0) {
            continue;
        }

        /* Skip draft posts in RSS */
        if (entry->draft) {
            continue;
        }

        entry_url = cxo_entry_url(arena, entry);
        if (!entry_url) {
            entry_url = "";
        }

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
                "<link>%s%s</link>\n"
                "<guid>%s%s</guid>\n"
                "<pubDate>%s</pubDate>\n"
                "<description><![CDATA[%s]]></description>\n"
                "</item>\n",
                item_title, base_url, entry_url,
                base_url, entry_url,
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
    size_t i;

    fprintf(fp,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n");
    for (i = 0; i < CXO_LANG_COUNT; i++) {
        const cxo_lang_t* l = &CXO_LANGS[i];
        if (l->prefix[0]) {
            fprintf(fp,
                    "<url>\n"
                    "<loc>%s/%s/</loc>\n"
                    "<priority>1.0</priority>\n"
                    "</url>\n", base_url, l->prefix);
        } else {
            fprintf(fp,
                    "<url>\n"
                    "<loc>%s/</loc>\n"
                    "<priority>1.0</priority>\n"
                    "</url>\n", base_url);
        }
    }
}

/* Write a single sitemap entry */
static void write_sitemap_entry(FILE* fp, cxo_entry_t* entry,
                                const char* base_url, arena_t* arena)
{
    char* entry_url = cxo_entry_url(arena, entry);
    fprintf(fp,
            "<url>\n"
            "<loc>%s%s</loc>\n"
            "<lastmod>%s</lastmod>\n"
            "<priority>0.8</priority>\n"
            "</url>\n",
            base_url, entry_url ? entry_url : "", entry->date);
}

/* Generate sitemap */
int render_sitemap(cxo_context_t* ctx, arena_t* arena,
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
        write_sitemap_entry(fp, entry, base_url, arena);
    }
    
    /* Add paginated index pages to sitemap */
    {
        size_t li;
        size_t p;

        for (li = 0; li < CXO_LANG_COUNT; li++) {
            const cxo_lang_t* l = &CXO_LANGS[li];
            size_t count = count_matching_entries(ctx, l->code, 0);
            size_t pages = ctx->posts_per_page > 0 ?
                (count + ctx->posts_per_page - 1) / ctx->posts_per_page : 1;

            for (p = 2; p <= pages; p++) {
                if (l->prefix[0]) {
                    fprintf(fp,
                            "<url>\n"
                            "<loc>%s/%s/page/%lu/</loc>\n"
                            "<priority>0.6</priority>\n"
                            "</url>\n",
                            base_url, l->prefix, (unsigned long)p);
                } else {
                    fprintf(fp,
                            "<url>\n"
                            "<loc>%s/page/%lu/</loc>\n"
                            "<priority>0.6</priority>\n"
                            "</url>\n",
                            base_url, (unsigned long)p);
                }
            }
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
            size_t li;
            for (li = 0; li < CXO_LANG_COUNT; li++) {
                const cxo_lang_t* l = &CXO_LANGS[li];
                if (l->prefix[0]) {
                    fprintf(fp,
                            "<url>\n"
                            "<loc>%s/%s/%s/</loc>\n"
                            "<priority>0.5</priority>\n"
                            "</url>\n",
                            base_url, l->prefix, years[j]);
                } else {
                    fprintf(fp,
                            "<url>\n"
                            "<loc>%s/%s/</loc>\n"
                            "<priority>0.5</priority>\n"
                            "</url>\n",
                            base_url, years[j]);
                }
            }
        }

        for (j = 0; j < month_count; j++) {
            size_t li;
            for (li = 0; li < CXO_LANG_COUNT; li++) {
                const cxo_lang_t* l = &CXO_LANGS[li];
                if (l->prefix[0]) {
                    fprintf(fp,
                            "<url>\n"
                            "<loc>%s/%s/%c%c%c%c/%c%c/</loc>\n"
                            "<priority>0.5</priority>\n"
                            "</url>\n",
                            base_url, l->prefix,
                            months[j][0], months[j][1], months[j][2],
                            months[j][3], months[j][5], months[j][6]);
                } else {
                    fprintf(fp,
                            "<url>\n"
                            "<loc>%s/%c%c%c%c/%c%c/</loc>\n"
                            "<priority>0.5</priority>\n"
                            "</url>\n",
                            base_url,
                            months[j][0], months[j][1], months[j][2],
                            months[j][3], months[j][5], months[j][6]);
                }
            }
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

