/* Test scanner module */
#include "include/arena.h"
#include "include/cxo.h"
#include <stdio.h>

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
    
    if (ctx->count != 2) {
        printf("\nFAIL: Expected 2 entries, got %zu\n", ctx->count);
        arena_destroy(arena);
        return 1;
    }
    
    printf("\nPASS: Found expected 2 entries\n");
    
    arena_destroy(arena);
    printf("PASS: cleanup\n");
    
    printf("\nAll scanner tests passed!\n");
    return 0;
}
