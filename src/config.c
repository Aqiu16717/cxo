/*
 * config.c - Configuration File Parser (TOML)
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <string.h>
#include <toml.h>
#include "../include/cxo.h"

/* Helper to safely get string from TOML */
static char* toml_string_or(toml_table_t* tab, const char* key,
                             arena_t* arena, const char* def)
{
    const char* val;
    
    val = toml_raw_in(tab, key);
    if (!val) {
        return arena_strdup(arena, def);
    }
    
    /* toml_raw_in returns quoted string, strip quotes */
    if (val[0] == '"') {
        size_t len;
        char* result;
        
        len = strlen(val);
        if (len >= 2 && val[len - 1] == '"') {
            result = arena_alloc(arena, len - 1);
            memcpy(result, val + 1, len - 2);
            result[len - 2] = '\0';
            return result;
        }
    }
    
    return arena_strdup(arena, val);
}

/* Load config from TOML file */
int cxo_load_config(cxo_context_t* ctx, arena_t* arena,
                    const char* config_path)
{
    FILE* fp;
    toml_table_t* conf;
    toml_table_t* site;
    char errbuf[256];
    
    /* Set defaults first */
    ctx->site_title = arena_strdup(arena, "CXO Blog");
    ctx->site_description = arena_strdup(arena, "A minimalist blog");
    ctx->base_url = arena_strdup(arena, "http://localhost");
    ctx->theme_path = arena_strdup(arena, "themes/default");
    
    /* Open config file */
    fp = fopen(config_path, "r");
    if (!fp) {
        /* File not found - use defaults */
        return CXO_OK;
    }
    
    /* Parse TOML */
    conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);
    
    if (!conf) {
        fprintf(stderr, "Warning: Failed to parse %s: %s\n", config_path, errbuf);
        return CXO_OK;  /* Use defaults on parse error */
    }
    
    /* Read [site] section */
    site = toml_table_in(conf, "site");
    if (site) {
        ctx->site_title = toml_string_or(site, "title", arena, ctx->site_title);
        ctx->site_description = toml_string_or(site, "description", arena,
                                                ctx->site_description);
        ctx->base_url = toml_string_or(site, "base_url", arena, ctx->base_url);
    }
    
    /* Read [theme] section */
    site = toml_table_in(conf, "theme");
    if (site) {
        ctx->theme_path = toml_string_or(site, "path", arena, ctx->theme_path);
    }
    
    toml_free(conf);
    return CXO_OK;
}
