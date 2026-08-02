/*
 * template.c - Template loading, variable substitution and escaping
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "renderer_internal.h"

/* Hot reload script - injected when CXO_HOTRELOAD=1 */
const char* hotreload_script =
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
    "{{meta_tags}}\n"
    "<title>{{site_title}} - {{archive_title}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var s = localStorage.getItem('cxo-theme');\n"
    "  var d = window.matchMedia('(prefers-color-scheme: dark)').matches;\n"
    "  if (s === 'dark' || (!s && d)) h.setAttribute('data-theme', 'dark');\n"
    "})();\n"
    "</script>\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a></nav>\n"
    "<h1>{{archive_title}}</h1>\n"
    "<ul class=\"post-list\">\n"
    "{{entry_list}}"
    "</ul>\n"
    "{{hotreload}}\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var b = document.createElement('button');\n"
    "  b.className = 'theme-toggle';\n"
    "  b.textContent = h.getAttribute('data-theme') === 'dark' ? '☀️' : '🌙';\n"
    "  b.onclick = function() {\n"
    "    if (h.getAttribute('data-theme') === 'dark') {\n"
    "      h.removeAttribute('data-theme');\n"
    "      localStorage.setItem('cxo-theme', 'light');\n"
    "      b.textContent = '🌙';\n"
    "    } else {\n"
    "      h.setAttribute('data-theme', 'dark');\n"
    "      localStorage.setItem('cxo-theme', 'dark');\n"
    "      b.textContent = '☀️';\n"
    "    }\n"
    "  };\n"
    "  var n = document.querySelector('nav');\n"
    "  if (n) n.appendChild(b);\n"
    "})();\n"
    "</script>\n"
    "</body>\n"
    "</html>\n";

/* Fallback tag template */
static const char* fallback_tag_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "{{meta_tags}}\n"
    "<title>{{site_title}} - {{tag_name}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var s = localStorage.getItem('cxo-theme');\n"
    "  var d = window.matchMedia('(prefers-color-scheme: dark)').matches;\n"
    "  if (s === 'dark' || (!s && d)) h.setAttribute('data-theme', 'dark');\n"
    "})();\n"
    "</script>\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a></nav>\n"
    "<h1>{{tag_name}}</h1>\n"
    "<ul class=\"post-list\">\n"
    "{{entry_list}}"
    "</ul>\n"
    "{{hotreload}}\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var b = document.createElement('button');\n"
    "  b.className = 'theme-toggle';\n"
    "  b.textContent = h.getAttribute('data-theme') === 'dark' ? '☀️' : '🌙';\n"
    "  b.onclick = function() {\n"
    "    if (h.getAttribute('data-theme') === 'dark') {\n"
    "      h.removeAttribute('data-theme');\n"
    "      localStorage.setItem('cxo-theme', 'light');\n"
    "      b.textContent = '🌙';\n"
    "    } else {\n"
    "      h.setAttribute('data-theme', 'dark');\n"
    "      localStorage.setItem('cxo-theme', 'dark');\n"
    "      b.textContent = '☀️';\n"
    "    }\n"
    "  };\n"
    "  var n = document.querySelector('nav');\n"
    "  if (n) n.appendChild(b);\n"
    "})();\n"
    "</script>\n"
    "</body>\n"
    "</html>\n";

/* Fallback index template */
static const char* fallback_index_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "{{meta_tags}}\n"
    "<title>{{site_title}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var s = localStorage.getItem('cxo-theme');\n"
    "  var d = window.matchMedia('(prefers-color-scheme: dark)').matches;\n"
    "  if (s === 'dark' || (!s && d)) h.setAttribute('data-theme', 'dark');\n"
    "})();\n"
    "</script>\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a></nav>\n"
    "<h1>{{site_title}}</h1>\n"
    "<ul class=\"post-list\">\n"
    "{{entry_list}}"
    "</ul>\n"
    "{{pagination}}"
    "{{hotreload}}\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var b = document.createElement('button');\n"
    "  b.className = 'theme-toggle';\n"
    "  b.textContent = h.getAttribute('data-theme') === 'dark' ? '☀️' : '🌙';\n"
    "  b.onclick = function() {\n"
    "    if (h.getAttribute('data-theme') === 'dark') {\n"
    "      h.removeAttribute('data-theme');\n"
    "      localStorage.setItem('cxo-theme', 'light');\n"
    "      b.textContent = '🌙';\n"
    "    } else {\n"
    "      h.setAttribute('data-theme', 'dark');\n"
    "      localStorage.setItem('cxo-theme', 'dark');\n"
    "      b.textContent = '☀️';\n"
    "    }\n"
    "  };\n"
    "  var n = document.querySelector('nav');\n"
    "  if (n) n.appendChild(b);\n"
    "})();\n"
    "</script>\n"
    "</body>\n"
    "</html>\n";

/* Fallback inline template */
static const char* fallback_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "{{meta_tags}}\n"
    "<title>{{title}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/styles/github.min.css\">\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var s = localStorage.getItem('cxo-theme');\n"
    "  var d = window.matchMedia('(prefers-color-scheme: dark)').matches;\n"
    "  if (s === 'dark' || (!s && d)) h.setAttribute('data-theme', 'dark');\n"
    "})();\n"
    "</script>\n"
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
    "{{hotreload}}\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var b = document.createElement('button');\n"
    "  b.className = 'theme-toggle';\n"
    "  b.textContent = h.getAttribute('data-theme') === 'dark' ? '☀️' : '🌙';\n"
    "  b.onclick = function() {\n"
    "    if (h.getAttribute('data-theme') === 'dark') {\n"
    "      h.removeAttribute('data-theme');\n"
    "      localStorage.setItem('cxo-theme', 'light');\n"
    "      b.textContent = '🌙';\n"
    "    } else {\n"
    "      h.setAttribute('data-theme', 'dark');\n"
    "      localStorage.setItem('cxo-theme', 'dark');\n"
    "      b.textContent = '☀️';\n"
    "    }\n"
    "  };\n"
    "  var n = document.querySelector('nav');\n"
    "  if (n) n.appendChild(b);\n"
    "})();\n"
    "</script>\n"
    "</body>\n"
    "</html>\n";

/* Return pointer to the "}}" closing a {{key}} starting at s, or NULL */
static const char* parse_var_key(const char* s)
{
    const char* p = s;

    if (!isalnum((unsigned char)*p) && *p != '_') {
        return NULL;
    }
    while (isalnum((unsigned char)*p) || *p == '_') {
        p++;
    }
    if (p[0] == '}' && p[1] == '}') {
        return p;
    }
    return NULL;
}

/* Look up a variable value; NULL if the key is not in the table */
static const char* var_lookup(const cxo_var_t* vars, size_t var_count,
                              const char* key, size_t key_len)
{
    size_t i;

    for (i = 0; i < var_count; i++) {
        if (strlen(vars[i].key) == key_len &&
            strncmp(vars[i].key, key, key_len) == 0) {
            return vars[i].value ? vars[i].value : "";
        }
    }
    return NULL;
}

/* Warn about an unknown template variable, once per key per process */
static void warn_unknown_var(const char* key, size_t key_len)
{
    static char warned[16][64];
    static size_t warned_count;
    size_t i;

    if (key_len == 0 || key_len >= 64) {
        return;
    }
    for (i = 0; i < warned_count; i++) {
        if (strncmp(warned[i], key, key_len) == 0 && warned[i][key_len] == '\0') {
            return;
        }
    }
    if (warned_count < 16) {
        memcpy(warned[warned_count], key, key_len);
        warned[warned_count][key_len] = '\0';
        warned_count++;
    }
    fprintf(stderr, "Warning: unknown template variable {{%.*s}}\n",
            (int)key_len, key);
}

/* Substitute all {{key}} placeholders in one scan.
 * Unknown variables are kept verbatim and reported once per key.
 * Inserted values are never re-scanned. */
char* replace_vars(arena_t* arena, const char* tmpl,
                   const cxo_var_t* vars, size_t var_count)
{
    const char* p;
    char* result;
    char* w;
    size_t len;

    /* Pass 1: compute result length */
    len = 0;
    for (p = tmpl; *p; ) {
        if (p[0] == '{' && p[1] == '{') {
            const char* end = parse_var_key(p + 2);
            if (end) {
                size_t key_len = end - (p + 2);
                const char* val = var_lookup(vars, var_count, p + 2, key_len);
                if (val) {
                    len += strlen(val);
                } else {
                    warn_unknown_var(p + 2, key_len);
                    len += key_len + 4;
                }
                p = end + 2;
                continue;
            }
        }
        len++;
        p++;
    }

    result = arena_alloc(arena, len + 1);
    if (!result) {
        return NULL;
    }

    /* Pass 2: emit */
    w = result;
    for (p = tmpl; *p; ) {
        if (p[0] == '{' && p[1] == '{') {
            const char* end = parse_var_key(p + 2);
            if (end) {
                size_t key_len = end - (p + 2);
                const char* val = var_lookup(vars, var_count, p + 2, key_len);
                if (val) {
                    size_t val_len = strlen(val);
                    memcpy(w, val, val_len);
                    w += val_len;
                } else {
                    memcpy(w, p, key_len + 4);
                    w += key_len + 4;
                }
                p = end + 2;
                continue;
            }
        }
        *w++ = *p++;
    }
    *w = '\0';
    return result;
}

/* Load template */
char* load_template(arena_t* arena, const char* theme_path)
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
char* load_tag_template(arena_t* arena, const char* theme_path)
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
char* load_index_template(arena_t* arena, const char* theme_path)
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
char* load_archive_template(arena_t* arena, const char* theme_path)
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

/* Check if hot reload is enabled */
int hotreload_enabled(void)
{
    return getenv("CXO_HOTRELOAD") != NULL;
}

/* Escape a string for use in a double-quoted HTML attribute */
char* escape_attr(arena_t* arena, const char* src)
{
    size_t len;
    size_t i;
    size_t j;
    char* dst;

    if (!src) {
        return arena_strdup(arena, "");
    }

    len = strlen(src);
    dst = arena_alloc(arena, len * 6 + 1);
    if (!dst) {
        return NULL;
    }

    j = 0;
    for (i = 0; i < len; i++) {
        switch (src[i]) {
        case '<':
            memcpy(dst + j, "&lt;", 4);
            j += 4;
            break;
        case '>':
            memcpy(dst + j, "&gt;", 4);
            j += 4;
            break;
        case '&':
            memcpy(dst + j, "&amp;", 5);
            j += 5;
            break;
        case '"':
            memcpy(dst + j, "&quot;", 6);
            j += 6;
            break;
        default:
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
    return dst;
}

/* Check if draft posts should be rendered */
int show_drafts(void)
{
    return getenv("CXO_DRAFT") != NULL;
}

