/* Test scanner module */
#include "include/arena.h"
#include "include/cxo.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    arena_t* arena;
    cxo_context_t* ctx;
    int ret;
    
    printf("Testing scanner...\n\n");
    
    /* Create arena and context */
    arena = arena_create(4096);
    if (!arena) {
        printf("FAIL: arena_create\n");
        return 1;
    }
    printf("PASS: arena_create\n");
    
    ctx = cxo_context_create(arena);
    if (!ctx) {
        printf("FAIL: cxo_context_create\n");
        arena_destroy(arena);
        return 1;
    }
    printf("PASS: cxo_context_create\n");
    
    /* Test scanning */
    ret = cxo_scan_content(ctx, arena, "content");
    if (CXO_IS_ERR(ret)) {
        printf("FAIL: cxo_scan_content returned %d\n", ret);
        arena_destroy(arena);
        return 1;
    }
    printf("PASS: cxo_scan_content\n");
    
    /* Verify results */
    printf("\nFound %zu entries:\n", ctx->count);
    for (size_t i = 0; i < ctx->count; i++) {
        cxo_entry_t* e = ctx->entries[i];
        printf("  [%zu] id=%s, lang=%s, slug=%s, path=%s\n",
               i, e->id, e->lang, e->slug, e->md_content);
    }
    
    if (ctx->count < 2) {
        printf("\nFAIL: Expected at least 2 entries, got %zu\n", ctx->count);
        arena_destroy(arena);
        return 1;
    }
    
    /* Verify paths and basic fields */
    for (size_t i = 0; i < ctx->count; i++) {
        cxo_entry_t* e = ctx->entries[i];
        char expected_path[256];
        
        if (!e->id || !e->lang || !e->slug || !e->md_content) {
            printf("FAIL: Entry %zu has NULL fields\n", i);
            arena_destroy(arena);
            return 1;
        }
        
        snprintf(expected_path, sizeof(expected_path), "content/%s/%s.md",
                 e->lang, e->slug);
        if (strcmp(e->md_content, expected_path) != 0) {
            printf("FAIL: Entry %zu path mismatch: expected %s, got %s\n",
                   i, expected_path, e->md_content);
            arena_destroy(arena);
            return 1;
        }
        
        if (strcmp(e->id, e->slug) != 0) {
            printf("FAIL: Entry %zu id != slug: %s vs %s\n",
                   i, e->id, e->slug);
            arena_destroy(arena);
            return 1;
        }
    }
    printf("PASS: All entry paths and fields correct\n");
    
    printf("\nPASS: Found expected %zu entries\n", ctx->count);
    
    arena_destroy(arena);
    printf("PASS: cleanup\n");
    
    printf("\nAll scanner tests passed!\n");
    return 0;
}
