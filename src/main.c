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

/* External command functions */
extern int cmd_init(const char* dir);
extern int cmd_new(const char* title);
extern int cmd_clean(void);

static void print_usage(const char* prog)
{
    fprintf(stderr, "Usage: %s <command> [options]\n", prog);
    fprintf(stderr, "\nCommands:\n");
    fprintf(stderr, "  init [dir]   Initialize a new CXO project\n");
    fprintf(stderr, "  new <title>  Create a new blog post\n");
    fprintf(stderr, "  build        Build the static site\n");
    fprintf(stderr, "  clean        Clean build output\n");
    fprintf(stderr, "  version      Show version information\n");
    fprintf(stderr, "  help         Show this help message\n");
}

static void print_version(void)
{
    printf("CXO %s - Minimalist Static Blog Engine\n", CXO_VERSION);
    printf("Copyright (c) 2026 Aq!u\n");
    printf("MIT License\n");
}

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

/* Process all entries: parse and link */
static void process_entries(cxo_context_t* ctx, arena_t* arena)
{
    int ret;
    size_t i;
    
    /* Parse all markdown files */
    printf("Parsing markdown...\n");
    for (i = 0; i < ctx->count; i++) {
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
}

static int do_build(void)
{
    arena_t* arena;
    cxo_context_t* ctx;
    int ret;
    
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
    
    printf("Found %zu entries\n", ctx->count);
    
    process_entries(ctx, arena);
    
    /* Render site */
    printf("Rendering site...\n");
    ret = cxo_render_site(ctx, arena, "public");
    if (CXO_IS_ERR(ret)) {
        fprintf(stderr, "Error: Failed to render site\n");
        arena_destroy(arena);
        return ret;
    }
    
    printf("Build complete! Output: public/\n");
    
    arena_destroy(arena);
    return CXO_OK;
}

int main(int argc, char* argv[])
{
    const char* cmd;
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    cmd = argv[1];
    
    if (strcmp(cmd, "init") == 0) {
        /* cxo init [dir] */
        const char* dir = (argc >= 3) ? argv[2] : ".";
        int rc = cmd_init(dir);
        return CXO_IS_ERR(rc) ? 1 : 0;
    } else if (strcmp(cmd, "new") == 0) {
        /* cxo new <title> */
        if (argc < 3) {
            fprintf(stderr, "Error: Missing title\n");
            return 1;
        }
        int rc = cmd_new(argv[2]);
        return CXO_IS_ERR(rc) ? 1 : 0;
    } else if (strcmp(cmd, "clean") == 0) {
        int rc = cmd_clean();
        return CXO_IS_ERR(rc) ? 1 : 0;
    } else if (strcmp(cmd, "build") == 0 || strcmp(cmd, "g") == 0) {
        int rc = do_build();
        return CXO_IS_ERR(rc) ? 1 : 0;
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
