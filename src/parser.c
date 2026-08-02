/*
 * parser.c - Markdown Parser and Front-matter Extractor
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/cmark.h"
#include "../include/cxo.h"

#define MAX_LINE_LEN 8192
#define MAX_CONTENT_SIZE (1024 * 1024)  /* 1MB max per file */

/* Read entire file into memory */
static char* read_file(arena_t* arena, const char* filepath)
{
    FILE* fp;
    long size;
    char* content;
    size_t read_size;
    
    fp = fopen(filepath, "r");
    if (!fp) {
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size <= 0 || size > MAX_CONTENT_SIZE) {
        fclose(fp);
        return NULL;
    }
    
    content = arena_alloc(arena, size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }
    
    read_size = fread(content, 1, size, fp);
    fclose(fp);
    
    if (read_size != (size_t)size) {
        return NULL;
    }
    
    content[size] = '\0';
    return content;
}

/* Trim whitespace from both ends */
static char* trim(char* str)
{
    char* end;
    
    /* Trim leading whitespace */
    while (isspace((unsigned char)*str)) {
        str++;
    }
    
    if (*str == '\0') {
        return str;
    }
    
    /* Trim trailing whitespace */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    
    end[1] = '\0';
    return str;
}

/* Parse comma-separated tags */
static int parse_tags(cxo_entry_t* entry, arena_t* arena, const char* value)
{
    char* buf;
    char* p;
    char* start;
    size_t count;
    size_t i;
    size_t len;
    char** tags;
    
    if (!value || !*value) {
        entry->tags = NULL;
        entry->tag_count = 0;
        return CXO_OK;
    }
    
    len = strlen(value);
    buf = arena_alloc(arena, len + 1);
    if (!buf) {
        return CXO_ERR_NOMEM;
    }
    memcpy(buf, value, len + 1);
    
    /* Strip surrounding [] if present */
    p = buf;
    while (isspace((unsigned char)*p)) {
        p++;
    }
    if (*p == '[') {
        char* endp;
        p++;
        endp = buf + len - 1;
        while (endp > p && isspace((unsigned char)*endp)) {
            endp--;
        }
        if (*endp == ']') {
            *endp = '\0';
        }
    }
    
    /* Count commas to determine max tags */
    count = 1;
    for (i = 0; p[i]; i++) {
        if (p[i] == ',') {
            count++;
        }
    }
    
    tags = arena_calloc_count(arena, count, sizeof(char*));
    if (!tags) {
        return CXO_ERR_NOMEM;
    }
    
    /* Split by comma */
    count = 0;
    start = p;
    while (*start) {
        char* comma = strchr(start, ',');
        if (comma) {
            *comma = '\0';
        }
        tags[count] = arena_strdup(arena, trim(start));
        if (tags[count] && strlen(tags[count]) > 0) {
            count++;
        }
        if (!comma) {
            break;
        }
        start = comma + 1;
    }
    
    entry->tags = tags;
    entry->tag_count = count;
    return CXO_OK;
}

/* Parse a single front-matter line: "key: value" */
static int parse_frontmatter_line(cxo_entry_t* entry, arena_t* arena,
                                  const char* line)
{
    const char* colon;
    char* key;
    char* value;
    size_t key_len;
    
    colon = strchr(line, ':');
    if (!colon) {
        return -1;
    }
    
    /* Extract key */
    key_len = colon - line;
    key = arena_alloc(arena, key_len + 1);
    if (!key) {
        return -1;
    }
    memcpy(key, line, key_len);
    key[key_len] = '\0';
    
    /* Extract and trim value */
    value = trim((char*)colon + 1);
    
    /* Store in entry */
    if (strcmp(key, "id") == 0) {
        entry->id = arena_strdup(arena, value);
    } else if (strcmp(key, "title") == 0) {
        entry->title = arena_strdup(arena, value);
    } else if (strcmp(key, "date") == 0) {
        entry->date = arena_strdup(arena, value);
    } else if (strcmp(key, "slug") == 0) {
        entry->slug = arena_strdup(arena, value);
    } else if (strcmp(key, "draft") == 0) {
        /* Parse draft: true/false/yes/no/1/0 */
        if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0 ||
            strcmp(value, "1") == 0) {
            entry->draft = 1;
        } else {
            entry->draft = 0;
        }
    } else if (strcmp(key, "description") == 0) {
        entry->description = arena_strdup(arena, value);
    } else if (strcmp(key, "tags") == 0) {
        parse_tags(entry, arena, value);
    }
    
    return CXO_OK;
}

/* Parse single frontmatter line, returns new content position */
static char* parse_frontmatter_content_line(cxo_entry_t* entry, arena_t* arena,
                                            char* content, int* in_fm)
{
    char* line_end;
    char* line;
    size_t line_len;
    
    line_end = strchr(content, '\n');
    if (!line_end) {
        *in_fm = 0;
        return content;
    }
    
    line_len = line_end - content;
    
    /* Check for closing --- */
    if (line_len >= 3 && strncmp(content, "---", 3) == 0) {
        *in_fm = 0;
        return line_end + 1;
    }
    
    /* Copy line for parsing */
    line = arena_alloc(arena, line_len + 1);
    memcpy(line, content, line_len);
    line[line_len] = '\0';
    
    parse_frontmatter_line(entry, arena, line);
    
    return line_end + 1;
}

/* Skip frontmatter opening delimiter --- */
static char* skip_frontmatter_opening(char* content)
{
    content += 3;
    if (*content == '\r') {
        content++;
    }
    return content + 1;
}

/* Forward declaration */
void cxo_generate_toc(cxo_entry_t* entry, arena_t* arena,
                      const char* md_source);

static int cxo_parse_frontmatter(cxo_entry_t* entry, arena_t* arena,
                          char* content, char** content_start)
{
    int in_frontmatter;
    int line_count;
    
    /* Check for YAML frontmatter delimiter --- */
    if (strncmp(content, "---\n", 4) != 0 && 
        strncmp(content, "---\r\n", 5) != 0) {
        *content_start = content;
        return 0;
    }
    
    content = skip_frontmatter_opening(content);
    
    in_frontmatter = 1;
    line_count = 0;
    
    while (in_frontmatter && *content && line_count < 100) {
        content = parse_frontmatter_content_line(entry, arena, content, 
                                                  &in_frontmatter);
        line_count++;
    }
    
    *content_start = content;
    return CXO_OK;
}

/* Days in a month, with leap-year handling for February */
static int days_in_month(int year, int month)
{
    static const int days[] = { 31, 28, 31, 30, 31, 30,
                                31, 31, 30, 31, 30, 31 };

    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) ||
                       year % 400 == 0)) {
        return 29;
    }
    return days[month - 1];
}

/* Normalize date to zero-padded YYYY-MM-DD so strcmp-based sorting and
 * archive grouping stay correct (e.g. "2026-3-1" -> "2026-03-01").
 * Accepts YYYY and YYYY-MM (day/month default to 1). A full date with
 * trailing characters (e.g. ISO-8601 time) keeps its date part with a
 * warning. Anything else warns and falls back to the default. */
static void normalize_date(cxo_entry_t* entry, arena_t* arena)
{
    int year = 1;
    int month = 1;
    int day = 1;
    int fields;
    int n = 0;
    const char* rest;
    char normalized[16];

    if (!entry->date) {
        return;
    }

    fields = sscanf(entry->date, "%d-%d-%d%n", &year, &month, &day, &n);
    if (fields == 2) {
        /* %n above only runs on a full match; rescan shorter forms */
        sscanf(entry->date, "%d-%d%n", &year, &month, &n);
    } else if (fields == 1) {
        sscanf(entry->date, "%d%n", &year, &n);
    }
    rest = entry->date + n;

    if (fields < 1 || year < 1000 || year > 9999 ||
        (fields >= 2 && (month < 1 || month > 12)) ||
        (fields >= 3 && (day < 1 || day > days_in_month(year, month))) ||
        (fields < 3 && *rest != '\0')) {
        fprintf(stderr, "Warning: Invalid date '%s' in entry %s, "
                "using 1970-01-01\n", entry->date, entry->id);
        entry->date = arena_strdup(arena, "1970-01-01");
        return;
    }

    if (*rest != '\0') {
        fprintf(stderr, "Warning: Date '%s' in entry %s has trailing "
                "characters, using date part only\n",
                entry->date, entry->id);
    }

    snprintf(normalized, sizeof(normalized), "%04d-%02d-%02d",
             year, month, day);
    if (strcmp(normalized, entry->date) != 0) {
        entry->date = arena_strdup(arena, normalized);
    }
}

/* Set fallback values for fields the renderer relies on */
static void set_entry_defaults(cxo_entry_t* entry, arena_t* arena)
{
    if (!entry->title) {
        entry->title = arena_strdup(arena, "Untitled");
    }
    if (!entry->date) {
        entry->date = arena_strdup(arena, "1970-01-01");
    }
    if (!entry->html_content) {
        entry->html_content = arena_strdup(arena, "");
    }
    if (!entry->toc) {
        entry->toc = arena_strdup(arena, "");
    }
}

int cxo_parse_markdown(cxo_entry_t* entry, arena_t* arena,
                       const char* filepath)
{
    char* file_content;
    char* body_start;
    char* html;

    /* Read file if filepath provided, else use stored md_content as path */
    if (filepath) {
        file_content = read_file(arena, filepath);
    } else {
        file_content = read_file(arena, entry->md_content);
    }

    if (!file_content) {
        fprintf(stderr, "Error: Cannot read file %s\n",
                filepath ? filepath : entry->md_content);
        /* Keep the entry renderable so downstream stages survive
         * a failed parse; the error is reported by the caller. */
        set_entry_defaults(entry, arena);
        return CXO_ERR_NOFILE;
    }
    
    /* Keep original path, store content separately */
    /* entry->md_content still holds the file path */
    
    /* Parse frontmatter */
    body_start = file_content;
    if (cxo_parse_frontmatter(entry, arena, file_content, &body_start) != 0) {
        fprintf(stderr, "Warning: Failed to parse frontmatter in %s\n",
                filepath ? filepath : entry->md_content);
    }
    
    /* Set defaults */
    normalize_date(entry, arena);
    set_entry_defaults(entry, arena);
    if (!entry->id) {
        entry->id = entry->slug;
    }
    
    /* Convert markdown to HTML using libcmark */
    html = cmark_markdown_to_html(body_start, strlen(body_start), 0);
    if (!html) {
        fprintf(stderr, "Error: Failed to parse markdown\n");
        set_entry_defaults(entry, arena);
        return CXO_ERR_PARSE;
    }
    
    /* Copy HTML to arena */
    entry->html_content = arena_strdup(arena, html);
    
    /* libcmark allocates with malloc; we must free it explicitly.
     * This is the only exception to the arena-only rule in core logic.
     */
    free(html);
    
    /* Generate table of contents and add heading IDs */
    cxo_generate_toc(entry, arena, body_start);
    
    return CXO_OK;
}

#define MAX_HEADINGS 64

typedef struct {
    int level;
    char* text;
    char* id;
} heading_t;

/* Convert text to URL-friendly slug */
static void slugify(char* dst, const char* src, size_t dst_size)
{
    size_t i, j;
    int last_was_dash = 1;

    j = 0;
    for (i = 0; src[i] && j < dst_size - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        if (isalnum(c)) {
            dst[j++] = (char)tolower(c);
            last_was_dash = 0;
        } else if (c >= 0x80) {
            /* Preserve UTF-8 bytes so non-ASCII (e.g. Chinese)
             * headings get usable anchor ids */
            dst[j++] = (char)c;
            last_was_dash = 0;
        } else if (!last_was_dash && j > 0) {
            dst[j++] = '-';
            last_was_dash = 1;
        }
    }
    if (j > 0 && dst[j - 1] == '-') {
        j--;
    }
    dst[j] = '\0';
}

/* Collect plain text of a heading node's inline children.
 * Code spans contribute their literal text without the <code> tags. */
static void collect_heading_text(cmark_node* heading, char* buf, size_t size)
{
    cmark_iter* iter;
    cmark_event_type ev;
    size_t len = 0;

    iter = cmark_iter_new(heading);
    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
        cmark_node* node;
        cmark_node_type type;
        const char* lit;
        size_t lit_len;

        if (ev != CMARK_EVENT_ENTER) {
            continue;
        }
        node = cmark_iter_get_node(iter);
        type = cmark_node_get_type(node);
        if (type != CMARK_NODE_TEXT && type != CMARK_NODE_CODE) {
            continue;
        }
        lit = cmark_node_get_literal(node);
        if (!lit) {
            continue;
        }
        lit_len = strlen(lit);
        if (len + lit_len >= size) {
            lit_len = size - 1 - len;
        }
        memcpy(buf + len, lit, lit_len);
        len += lit_len;
    }
    cmark_iter_free(iter);
    buf[len] = '\0';
}

/* Collect headings (level, plain text, slug id) from the markdown AST.
 * Returns the number of headings found (capped at max). */
static size_t collect_headings(const char* md_source, heading_t* headings,
                               size_t max, arena_t* arena)
{
    cmark_node* doc;
    cmark_iter* iter;
    cmark_event_type ev = CMARK_EVENT_NONE;
    size_t count = 0;

    doc = cmark_parse_document(md_source, strlen(md_source), 0);
    if (!doc) {
        return 0;
    }

    iter = cmark_iter_new(doc);
    while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE &&
           count < max) {
        cmark_node* node;
        char text[512];
        size_t id_size;

        if (ev != CMARK_EVENT_ENTER) {
            continue;
        }
        node = cmark_iter_get_node(iter);
        if (cmark_node_get_type(node) != CMARK_NODE_HEADING) {
            continue;
        }

        collect_heading_text(node, text, sizeof(text));
        id_size = strlen(text) + 32;

        headings[count].level = cmark_node_get_heading_level(node);
        headings[count].text = arena_strdup(arena, text);
        headings[count].id = arena_alloc(arena, id_size);
        if (!headings[count].text || !headings[count].id) {
            break;
        }
        slugify(headings[count].id, text, id_size);
        if (headings[count].id[0] == '\0') {
            /* Heading with no slugifiable chars (e.g. punctuation only) */
            snprintf(headings[count].id, id_size,
                     "heading-%lu", (unsigned long)(count + 1));
        }
        count++;
    }

    if (ev != CMARK_EVENT_DONE) {
        fprintf(stderr, "Warning: heading limit (%lu) reached, TOC truncated\n",
                (unsigned long)max);
    }

    cmark_iter_free(iter);
    cmark_node_free(doc);
    return count;
}

/* Give duplicate heading ids numeric suffixes */
static void dedup_heading_ids(heading_t* headings, size_t hcount)
{
    size_t i;

    for (i = 0; i < hcount; i++) {
        size_t id_buf_size = strlen(headings[i].text) + 32;
        int suffix = 1;
        char base_id[256];

        strncpy(base_id, headings[i].id, sizeof(base_id) - 1);
        base_id[sizeof(base_id) - 1] = '\0';

        while (1) {
            size_t k;
            int dup = 0;
            for (k = 0; k < i; k++) {
                if (strcmp(headings[k].id, headings[i].id) == 0) {
                    dup = 1;
                    break;
                }
            }
            if (!dup) {
                break;
            }
            snprintf(headings[i].id, id_buf_size, "%s-%d",
                     base_id, ++suffix);
        }
    }
}

/* Inject id attributes into the rendered HTML headings.
 * Headings are matched positionally: the i-th <hN> tag in the HTML
 * corresponds to the i-th heading in the AST (same source, same order). */
static char* inject_heading_ids(const char* html, heading_t* headings,
                                size_t hcount, arena_t* arena)
{
    size_t html_len = strlen(html);
    size_t extra = 0;
    size_t idx = 0;
    size_t i;
    size_t mod_offset = 0;
    const char* p;
    const char* last;
    char* modified;

    for (i = 0; i < hcount; i++) {
        extra += strlen(headings[i].id) + 16;
    }

    modified = arena_alloc(arena, html_len + extra + 1);
    if (!modified) {
        return NULL;
    }

    last = html;
    for (p = html; *p && idx < hcount; ) {
        if (*p == '<' && p[1] == 'h' && p[2] >= '1' && p[2] <= '6' &&
            (p[3] == '>' || isspace((unsigned char)p[3]))) {
            const char* tag_end = strchr(p, '>');
            size_t segment;

            if (!tag_end) {
                break;
            }

            /* Copy everything before this heading tag */
            segment = p - last;
            memcpy(modified + mod_offset, last, segment);
            mod_offset += segment;

            /* Write opening tag with id */
            if (p[3] == '>') {
                mod_offset += snprintf(modified + mod_offset,
                                       html_len + extra - mod_offset,
                                       "<h%c id=\"%s\">", p[2],
                                       headings[idx].id);
            } else {
                size_t tag_len = tag_end - p + 1;
                memcpy(modified + mod_offset, p, tag_len - 1);
                mod_offset += tag_len - 1;
                mod_offset += snprintf(modified + mod_offset,
                                       html_len + extra - mod_offset,
                                       " id=\"%s\">", headings[idx].id);
            }

            last = tag_end + 1;
            p = tag_end + 1;
            idx++;
        } else {
            p++;
        }
    }

    /* Copy remainder */
    strcpy(modified + mod_offset, last);
    return modified;
}

/* Generate TOC from markdown headings and add id attributes */
void cxo_generate_toc(cxo_entry_t* entry, arena_t* arena,
                      const char* md_source)
{
    heading_t headings[MAX_HEADINGS];
    size_t hcount;
    size_t i;
    char* modified;
    char* toc;
    size_t toc_len;
    size_t toc_offset = 0;
    int prev_level = 0;

    if (!entry->html_content || !md_source) {
        entry->toc = arena_strdup(arena, "");
        return;
    }

    hcount = collect_headings(md_source, headings, MAX_HEADINGS, arena);
    if (hcount == 0) {
        entry->toc = arena_strdup(arena, "");
        return;
    }

    dedup_heading_ids(headings, hcount);

    modified = inject_heading_ids(entry->html_content, headings, hcount,
                                  arena);
    if (!modified) {
        entry->toc = arena_strdup(arena, "");
        return;
    }
    entry->html_content = modified;

    /* Build TOC HTML, sized from actual content */
    toc_len = 128;
    for (i = 0; i < hcount; i++) {
        toc_len += strlen(headings[i].text) + strlen(headings[i].id) + 64;
    }
    toc = arena_alloc(arena, toc_len);
    if (!toc) {
        entry->toc = arena_strdup(arena, "");
        return;
    }

    toc_offset = snprintf(toc, toc_len, "<nav class=\"toc\">\n<ul>\n");

    for (i = 0; i < hcount; i++) {
        int level = headings[i].level;

        if (i == 0) {
            toc_offset += snprintf(toc + toc_offset, toc_len - toc_offset,
                                   "<li>");
        } else if (level > prev_level) {
            int diff = level - prev_level;
            while (diff-- > 0) {
                toc_offset += snprintf(toc + toc_offset, toc_len - toc_offset,
                                       "<ul>\n<li>");
            }
        } else if (level < prev_level) {
            int diff = prev_level - level;
            toc_offset += snprintf(toc + toc_offset, toc_len - toc_offset,
                                   "</li>\n");
            while (diff-- > 0) {
                toc_offset += snprintf(toc + toc_offset, toc_len - toc_offset,
                                       "</ul>\n</li>\n");
            }
            toc_offset += snprintf(toc + toc_offset, toc_len - toc_offset,
                                   "<li>");
        } else {
            toc_offset += snprintf(toc + toc_offset, toc_len - toc_offset,
                                   "</li>\n<li>");
        }

        toc_offset += snprintf(toc + toc_offset, toc_len - toc_offset,
                               "<a href=\"#%s\">%s</a>",
                               headings[i].id, headings[i].text);
        prev_level = level;
    }

    toc_offset += snprintf(toc + toc_offset, toc_len - toc_offset,
                           "</li>\n");
    while (prev_level > 1) {
        toc_offset += snprintf(toc + toc_offset, toc_len - toc_offset,
                               "</ul>\n</li>\n");
        prev_level--;
    }
    toc_offset += snprintf(toc + toc_offset, toc_len - toc_offset,
                           "</ul>\n</nav>\n");

    entry->toc = toc;
}
