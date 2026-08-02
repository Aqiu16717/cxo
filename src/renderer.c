/*
 * renderer.c - HTML Template Renderer - site render orchestrator
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include "renderer_internal.h"

/* Render entire site */
int cxo_render_site(cxo_context_t* ctx, arena_t* arena,
                    const char* output_dir)
{
    size_t i;
    int rc;
    int success;
    char* tmpl;
    
    rc = ensure_dir(output_dir);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Error: Cannot create output directory\n");
        return rc;
    }
    
    /* Load template and copy assets */
    tmpl = load_template(arena, ctx->theme_path);
    rc = copy_theme_assets(ctx->theme_path, output_dir);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Warning: Failed to copy theme assets\n");
    }
    
    /* Sort entries by date descending and assign prev/next navigation */
    sort_entries(ctx);
    assign_prev_next(ctx);
    
    /* Render individual entries */
    success = 0;
    for (i = 0; i < ctx->count; i++) {
        rc = render_entry(ctx->entries[i], ctx, arena, output_dir, tmpl);
        if (!CXO_IS_ERR(rc)) {
            success++;
        }
    }
    
    /* Generate index pages */
    {
        char* index_tmpl = load_index_template(arena, ctx->theme_path);
        for (i = 0; i < CXO_LANG_COUNT; i++) {
            rc = render_index(ctx, arena, output_dir, CXO_LANGS[i].code,
                              index_tmpl);
            if (CXO_IS_ERR(rc)) {
                return rc;
            }
        }
    }

    /* Generate tag pages */
    {
        char* tag_tmpl = load_tag_template(arena, ctx->theme_path);
        char* unique_tags[MAX_UNIQUE_TAGS];
        size_t tag_count;
        size_t t;

        tag_count = collect_unique_tags(ctx, unique_tags);
        for (t = 0; t < tag_count; t++) {
            for (i = 0; i < CXO_LANG_COUNT; i++) {
                rc = render_tag_page(ctx, arena, output_dir, CXO_LANGS[i].code,
                                     unique_tags[t], tag_tmpl);
                if (CXO_IS_ERR(rc)) {
                    return rc;
                }
            }
        }
    }

    /* Generate archive pages */
    {
        char* archive_tmpl = load_archive_template(arena, ctx->theme_path);
        for (i = 0; i < CXO_LANG_COUNT; i++) {
            rc = render_archive_pages(ctx, arena, output_dir,
                                      CXO_LANGS[i].code, archive_tmpl);
            if (CXO_IS_ERR(rc)) {
                return rc;
            }
        }
    }

    /* Generate RSS feeds */
    for (i = 0; i < CXO_LANG_COUNT; i++) {
        rc = render_rss(ctx, arena, output_dir, CXO_LANGS[i].code);
        if (CXO_IS_ERR(rc)) {
            return rc;
        }
    }
    
    /* Generate sitemap */
    rc = render_sitemap(ctx, arena, output_dir);
    if (CXO_IS_ERR(rc)) {
        return rc;
    }
    
    /* Copy static assets */
    rc = copy_static_files(output_dir);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Warning: Failed to copy static files\n");
    }
    
    printf("Rendered %d/%lu entries\n", success, (unsigned long)ctx->count);
    
    return (success == (int)ctx->count) ? CXO_OK : CXO_ERR_RENDER;
}

