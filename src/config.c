/*
 * config.c - Configuration File Parser
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <string.h>
#include <ini.h>
#include "../include/cxo.h"

/* Config parsing context */
typedef struct {
    cxo_context_t* ctx;
    arena_t* arena;
} config_ctx_t;

/* Strip quotes from value if present */
static char* strip_quotes(arena_t* arena, const char* value)
{
    size_t len;
    char* result;
    
    len = strlen(value);
    if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
        /* Remove surrounding quotes */
        result = arena_alloc(arena, len - 1);
        memcpy(result, value + 1, len - 2);
        result[len - 2] = '\0';
        return result;
    }
    return arena_strdup(arena, value);
}

/* INI handler callback */
static int config_handler(void* user, const char* section,
                          const char* name, const char* value)
{
    config_ctx_t* cfg;
    char* clean_value;
    
    (void)section;
    
    cfg = (config_ctx_t*)user;
    clean_value = strip_quotes(cfg->arena, value);
    
    if (strcmp(name, "title") == 0) {
        cfg->ctx->site_title = clean_value;
    } else if (strcmp(name, "description") == 0) {
        cfg->ctx->site_description = clean_value;
    } else if (strcmp(name, "base_url") == 0) {
        cfg->ctx->base_url = clean_value;
    } else if (strcmp(name, "theme") == 0) {
        cfg->ctx->theme_path = clean_value;
    }
    
    return 1;
}

/* Load config from file */
int cxo_load_config(cxo_context_t* ctx, arena_t* arena,
                    const char* config_path)
{
    config_ctx_t cfg;
    int rc;
    
    /* Set defaults first */
    ctx->site_title = arena_strdup(arena, "CXO Blog");
    ctx->site_description = arena_strdup(arena, "A minimalist blog");
    ctx->base_url = arena_strdup(arena, "http://localhost");
    ctx->theme_path = arena_strdup(arena, "themes/default");
    
    cfg.ctx = ctx;
    cfg.arena = arena;
    
    rc = ini_parse(config_path, config_handler, &cfg);
    if (rc < 0) {
        /* File not found or error - defaults already set */
        return CXO_OK;
    }
    
    return CXO_OK;
}
