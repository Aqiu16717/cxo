/*
 * parser.c - Markdown Parser and Front-matter Extractor
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <cmark.h>
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

int cxo_parse_frontmatter(cxo_entry_t* entry, arena_t* arena,
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
    cxo_parse_frontmatter(entry, arena, file_content, &body_start);
    
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
    
    /* Free libcmark allocated string */
    free(html);
    
    return 0;
}
