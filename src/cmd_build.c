/*
 * cmd_build.c - Site Build Command
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include "../include/cxo.h"

#define DEFAULT_CHUNK_SIZE (1024 * 1024)  /* 1MB */

/* Initialize build context */
static int init_build_context(arena_t** arena, cxo_context_t** ctx)
{
    *arena = arena_create(DEFAULT_CHUNK_SIZE);
    if (!*arena) {
        fprintf(stderr, "Error: Failed to create arena allocator\n");
        return 1;
    }

    *ctx = cxo_context_create(*arena);
    if (!*ctx) {
        fprintf(stderr, "Error: Failed to create context\n");
        arena_destroy(*arena);
        return 1;
    }

    return 0;
}

/* Process all entries: parse and link. Returns the number of errors. */
static int process_entries(cxo_context_t* ctx, arena_t* arena)
{
    int ret;
    int errors = 0;
    size_t i;

    /* Parse all markdown files */
    printf("Parsing markdown...\n");
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        ret = cxo_parse_markdown(entry, arena, NULL);
        if (CXO_IS_ERR(ret)) {
            fprintf(stderr, "Warning: Failed to parse entry %s\n", entry->id);
            errors++;
        }
    }

    /* Link entries */
    printf("Linking entries...\n");
    ret = cxo_link_entries(ctx, arena);
    if (CXO_IS_ERR(ret)) {
        fprintf(stderr, "Warning: Failed to link some entries\n");
        errors++;
    }

    return errors;
}

/* Build the static site into public/ */
int cmd_build(void)
{
    arena_t* arena;
    cxo_context_t* ctx;
    int ret;
    int errors;

    if (init_build_context(&arena, &ctx) != 0) {
        return CXO_ERR_NOMEM;
    }

    /* Load config (uses defaults if file not found) */
    (void)cxo_load_config(ctx, arena, "config.toml");

    /* Scan content directories */
    printf("Scanning content...\n");
    ret = cxo_scan_content(ctx, arena, "content");
    if (CXO_IS_ERR(ret)) {
        fprintf(stderr, "Error: Failed to scan content\n");
        arena_destroy(arena);
        return ret;
    }

    printf("Found %lu entries\n", (unsigned long)ctx->count);

    errors = process_entries(ctx, arena);

    /* Render site */
    printf("Rendering site...\n");
    ret = cxo_render_site(ctx, arena, "public");
    if (CXO_IS_ERR(ret)) {
        fprintf(stderr, "Error: Failed to render site\n");
        arena_destroy(arena);
        return ret;
    }

    if (errors > 0) {
        fprintf(stderr, "Error: Build completed with %d error(s)\n", errors);
        arena_destroy(arena);
        return CXO_ERR_PARSE;
    }

    printf("Build complete! Output: public/\n");

    arena_destroy(arena);
    return CXO_OK;
}
