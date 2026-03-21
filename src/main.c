/*
 * main.c - CXO Static Blog Engine Main Entry
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../include/cxo.h"

#define DEFAULT_CHUNK_SIZE (1024 * 1024)  /* 1MB */

static void print_usage(const char* prog)
{
    fprintf(stderr, "Usage: %s <command> [options]\n", prog);
    fprintf(stderr, "\nCommands:\n");
    fprintf(stderr, "  build      Build the static site\n");
    fprintf(stderr, "  version    Show version information\n");
    fprintf(stderr, "  help       Show this help message\n");
}

static void print_version(void)
{
    printf("CXO %s - Minimalist Static Blog Engine\n", CXO_VERSION);
    printf("Copyright (c) 2026 Aq!u\n");
    printf("MIT License\n");
}

static int cmd_build(void)
{
    arena_t* arena;
    cxo_context_t* ctx;
    int ret;
    
    /* Initialize arena allocator */
    arena = arena_create(DEFAULT_CHUNK_SIZE);
    if (!arena) {
        fprintf(stderr, "Error: Failed to create arena allocator\n");
        return 1;
    }
    
    /* Create context */
    ctx = cxo_context_create(arena);
    if (!ctx) {
        fprintf(stderr, "Error: Failed to create context\n");
        arena_destroy(arena);
        return 1;
    }
    
    /* Load config (stub - uses defaults for now) */
    ctx->base_url = arena_strdup(arena, "http://localhost");
    ctx->theme_path = arena_strdup(arena, "themes/default");
    ctx->site_title = arena_strdup(arena, "CXO Blog");
    ctx->site_description = arena_strdup(arena, "A minimalist blog");
    
    /* Scan content directories */
    printf("Scanning content...\n");
    ret = cxo_scan_content(ctx, arena, "content");
    if (CXO_IS_ERR(ret)) {
        fprintf(stderr, "Error: Failed to scan content\n");
        arena_destroy(arena);
        return 1;
    }
    
    printf("Found %zu entries\n", ctx->count);
    
    /* Parse all markdown files */
    printf("Parsing markdown...\n");
    for (size_t i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        ret = cxo_parse_markdown(entry, arena, NULL);
        if (CXO_IS_ERR(ret)) {
            fprintf(stderr, "Warning: Failed to parse entry %s\n", entry->id);
        }
    }
    
    /* Link entries */
    printf("Linking entries...\n");
    ret = cxo_link_entries(ctx, arena);
    if (CXO_IS_ERR(ret)) {
        fprintf(stderr, "Warning: Failed to link some entries\n");
    }
    
    /* Render site */
    printf("Rendering site...\n");
    ret = cxo_render_site(ctx, arena, "public");
    if (CXO_IS_ERR(ret)) {
        fprintf(stderr, "Error: Failed to render site\n");
        arena_destroy(arena);
        return 1;
    }
    
    printf("Build complete! Output: public/\n");
    
    arena_destroy(arena);
    return 0;
}

int main(int argc, char* argv[])
{
    const char* cmd;
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    cmd = argv[1];
    
    if (strcmp(cmd, "build") == 0) {
        return cmd_build();
    } else if (strcmp(cmd, "version") == 0 || strcmp(cmd, "-v") == 0) {
        print_version();
        return 0;
    } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
}
