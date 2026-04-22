/*
 * config.c - Configuration File Parser (TOML)
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <toml.h>
#include "../include/cxo.h"

/* Helper to safely get string from TOML */
static char* toml_string_or(const toml_table_t* tab, const char* key,
                             arena_t* arena, const char* def)
{
    char* result;
    
    {
        toml_datum_t d = toml_string_in(tab, key);
        if (!d.ok) {
            return arena_strdup(arena, def);
        }
        result = arena_strdup(arena, d.u.s);
        free(d.u.s);
        return result;
    }
}

/* Helper to safely get int from TOML */
static long toml_int_or(const toml_table_t* tab, const char* key, long def)
{
    toml_datum_t d = toml_int_in(tab, key);
    if (!d.ok) {
        return def;
    }
    return d.u.i;
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
    ctx->posts_per_page = 0;
    
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
        {
            long pp = toml_int_or(site, "posts_per_page", 0);
            ctx->posts_per_page = (pp > 0) ? (size_t)pp : 0;
        }
    }
    
    /* Read [theme] section */
    site = toml_table_in(conf, "theme");
    if (site) {
        ctx->theme_path = toml_string_or(site, "path", arena, ctx->theme_path);
    }
    
    toml_free(conf);
    return CXO_OK;
}
