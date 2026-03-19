/* Test linker module */
#include "include/arena.h"
#include "include/cxo.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    arena_t* arena;
    cxo_context_t* ctx;
    int ret;
    
    printf("Testing linker...\n\n");
    
    /* Setup */
    arena = arena_create(4096);
    ctx = cxo_context_create(arena);
    if (!ctx || !arena) {
        printf("FAIL: setup\n");
        return 1;
    }
    printf("PASS: setup\n");
    
    /* Scan content */
    ret = cxo_scan_content(ctx, arena, "content");
    if (ret != 0 || ctx->count < 2) {
        printf("FAIL: need at least 2 entries to test linking\n");
        arena_destroy(arena);
        return 1;
    }
    printf("PASS: scanned %zu entries\n", ctx->count);
    
    /* Parse all entries first */
    for (size_t i = 0; i < ctx->count; i++) {
        ret = cxo_parse_markdown(ctx->entries[i], arena, NULL);
        if (ret != 0) {
            printf("FAIL: parse entry %zu\n", i);
            arena_destroy(arena);
            return 1;
        }
    }
    printf("PASS: parsed all entries\n");
    
    /* Show entries before linking */
    printf("\nEntries before linking:\n");
    for (size_t i = 0; i < ctx->count; i++) {
        cxo_entry_t* e = ctx->entries[i];
        printf("  [%zu] id=%s, lang=%s, peer=%p\n",
               i, e->id, e->lang, (void*)e->peer);
    }
    
    /* Test linking */
    printf("\nLinking entries...\n");
    ret = cxo_link_entries(ctx, arena);
    if (ret != 0) {
        printf("FAIL: cxo_link_entries returned %d\n", ret);
        arena_destroy(arena);
        return 1;
    }
    printf("PASS: cxo_link_entries\n");
    
    /* Verify linking results */
    printf("\nEntries after linking:\n");
    int linked = 0;
    for (size_t i = 0; i < ctx->count; i++) {
        cxo_entry_t* e = ctx->entries[i];
        printf("  [%zu] id=%s, lang=%s, peer=%s\n",
               i, e->id, e->lang,
               e->peer ? "yes" : "null");
        
        if (e->peer) {
            /* Verify bidirectional link */
            if (e->peer->peer != e) {
                printf("FAIL: broken link at entry %zu\n", i);
                arena_destroy(arena);
                return 1;
            }
            /* Verify same id */
            if (strcmp(e->id, e->peer->id) != 0) {
                printf("FAIL: mismatched id at entry %zu\n", i);
                arena_destroy(arena);
                return 1;
            }
            linked++;
        }
    }
    
    /* Each link is counted twice (once per entry) */
    if (linked != 2) {
        printf("\nFAIL: expected 2 linked entries, got %d\n", linked);
        arena_destroy(arena);
        return 1;
    }
    
    printf("\nPASS: all entries correctly linked (1 pair)\n");
    
    arena_destroy(arena);
    printf("PASS: cleanup\n");
    printf("\nAll linker tests passed!\n");
    return 0;
}
