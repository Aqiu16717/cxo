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
void cxo_generate_toc(cxo_entry_t* entry, arena_t* arena);

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
    if (!entry->title) {
        entry->title = arena_strdup(arena, "Untitled");
    }
    if (!entry->date) {
        entry->date = arena_strdup(arena, "1970-01-01");
    }
    if (!entry->id) {
        entry->id = entry->slug;
    }
    
    /* Convert markdown to HTML using libcmark */
    html = cmark_markdown_to_html(body_start, strlen(body_start), 0);
    if (!html) {
        fprintf(stderr, "Error: Failed to parse markdown\n");
        return CXO_ERR_PARSE;
    }
    
    /* Copy HTML to arena */
    entry->html_content = arena_strdup(arena, html);
    
    /* libcmark allocates with malloc; we must free it explicitly.
     * This is the only exception to the arena-only rule in core logic.
     */
    free(html);
    
    /* Generate table of contents and add heading IDs */
    cxo_generate_toc(entry, arena);
    
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

/* Generate TOC from HTML headings and add id attributes */
void cxo_generate_toc(cxo_entry_t* entry, arena_t* arena)
{
    const char* html = entry->html_content;
    size_t html_len;
    heading_t headings[MAX_HEADINGS];
    size_t hcount = 0;
    size_t extra_len = 0;
    const char* p;
    const char* last;
    char* modified;
    char* toc;
    size_t toc_len;
    size_t i;
    size_t mod_offset = 0;
    size_t toc_offset = 0;
    int prev_level = 0;
    
    if (!html) {
        entry->toc = arena_strdup(arena, "");
        return;
    }
    
    html_len = strlen(html);
    
    /* First pass: find all headings */
    p = html;
    while (*p && hcount < MAX_HEADINGS) {
        if (*p == '<' && p[1] == 'h' && p[2] >= '1' && p[2] <= '6' &&
            (p[3] == '>' || isspace((unsigned char)p[3]))) {
            int level = p[2] - '0';
            const char* tag_end = strchr(p, '>');
            const char* close_tag_start;
            char close_tag[8];
            size_t text_len;
            
            if (!tag_end) {
                break;
            }
            
            snprintf(close_tag, sizeof(close_tag), "</h%d>", level);
            close_tag_start = strstr(tag_end + 1, close_tag);
            if (!close_tag_start) {
                break;
            }
            
            text_len = close_tag_start - (tag_end + 1);
            
            headings[hcount].level = level;
            headings[hcount].text = arena_alloc(arena, text_len + 1);
            if (!headings[hcount].text) {
                entry->toc = arena_strdup(arena, "");
                return;
            }
            memcpy(headings[hcount].text, tag_end + 1, text_len);
            headings[hcount].text[text_len] = '\0';
            
            headings[hcount].id = arena_alloc(arena, text_len * 2 + 32);
            if (!headings[hcount].id) {
                entry->toc = arena_strdup(arena, "");
                return;
            }
            slugify(headings[hcount].id, headings[hcount].text,
                    text_len * 2 + 32);
            
            extra_len += strlen(headings[hcount].id) + 16;
            hcount++;
            p = close_tag_start + strlen(close_tag);
        } else {
            p++;
        }
    }
    
    if (hcount == 0) {
        entry->toc = arena_strdup(arena, "");
        return;
    }
    
    /* Fix duplicate IDs */
    for (i = 0; i < hcount; i++) {
        size_t id_buf_size = strlen(headings[i].text) * 2 + 32;
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
    
    /* Build modified HTML with id attributes */
    modified = arena_alloc(arena, html_len + extra_len + 1);
    if (!modified) {
        entry->toc = arena_strdup(arena, "");
        return;
    }
    
    last = html;
    hcount = 0;
    
    for (p = html; *p; ) {
        if (*p == '<' && p[1] == 'h' && p[2] >= '1' && p[2] <= '6' &&
            (p[3] == '>' || isspace((unsigned char)p[3]))) {
            int level = p[2] - '0';
            const char* tag_end = strchr(p, '>');
            const char* close_tag_start;
            char close_tag[8];
            size_t segment;
            
            if (!tag_end) {
                break;
            }
            
            snprintf(close_tag, sizeof(close_tag), "</h%d>", level);
            close_tag_start = strstr(tag_end + 1, close_tag);
            if (!close_tag_start) {
                break;
            }
            
            /* Copy everything before this heading tag */
            segment = p - last;
            memcpy(modified + mod_offset, last, segment);
            mod_offset += segment;
            
            /* Write opening tag with id */
            if (p[3] == '>') {
                mod_offset += snprintf(modified + mod_offset,
                                       html_len + extra_len - mod_offset,
                                       "<h%d id=\"%s\">",
                                       level, headings[hcount].id);
            } else {
                size_t tag_len = tag_end - p + 1;
                memcpy(modified + mod_offset, p, tag_len - 1);
                mod_offset += tag_len - 1;
                mod_offset += snprintf(modified + mod_offset,
                                       html_len + extra_len - mod_offset,
                                       " id=\"%s\">",
                                       headings[hcount].id);
            }
            
            last = tag_end + 1;
            p = close_tag_start + strlen(close_tag);
            hcount++;
        } else {
            p++;
        }
    }
    
    /* Copy remainder */
    strcpy(modified + mod_offset, last);
    entry->html_content = modified;
    
    /* Build TOC HTML */
    toc_len = hcount * 256 + 128;
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
