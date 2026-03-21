/* Test renderer module */
#include "include/arena.h"
#include "include/cxo.h"
#include <stdio.h>
#include <sys/stat.h>

int main(void)
{
    arena_t* arena;
    cxo_context_t* ctx;
    int rc;
    struct stat st;
    
    printf("Testing renderer...\n\n");
    
    /* Setup */
    arena = arena_create(4096);
    ctx = cxo_context_create(arena);
    if (!ctx || !arena) {
        printf("FAIL: setup\n");
        return 1;
    }
    printf("PASS: setup\n");
    
    /* Scan */
    rc = cxo_scan_content(ctx, arena, "content");
    if (CXO_IS_ERR(rc) || ctx->count < 2) {
        printf("FAIL: scan\n");
        arena_destroy(arena);
        return 1;
    }
    printf("PASS: scanned %zu entries\n", ctx->count);
    
    /* Parse */
    for (size_t i = 0; i < ctx->count; i++) {
        rc = cxo_parse_markdown(ctx->entries[i], arena, NULL);
        if (CXO_IS_ERR(rc)) {
            printf("FAIL: parse entry %zu\n", i);
            arena_destroy(arena);
            return 1;
        }
    }
    printf("PASS: parsed all entries\n");
    
    /* Link */
    rc = cxo_link_entries(ctx, arena);
    if (CXO_IS_ERR(rc)) {
        printf("FAIL: link\n");
        arena_destroy(arena);
        return 1;
    }
    printf("PASS: linked entries\n");
    
    /* Render */
    printf("\nRendering site...\n");
    rc = cxo_render_site(ctx, arena, "public");
    if (CXO_IS_ERR(rc)) {
        printf("FAIL: render\n");
        arena_destroy(arena);
        return 1;
    }
    printf("PASS: cxo_render_site\n");
    
    /* Verify output files exist */
    printf("\nVerifying output files...\n");
    if (stat("public/posts/hello.html", &st) != 0) {
        printf("FAIL: public/posts/hello.html not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/posts/hello.html\n");
    
    if (stat("public/en/posts/hello.html", &st) != 0) {
        printf("FAIL: public/en/posts/hello.html not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/en/posts/hello.html\n");
    
    arena_destroy(arena);
    printf("\nPASS: cleanup\n");
    printf("\nAll renderer tests passed!\n");
    printf("\nView generated files:\n");
    printf("  cat public/posts/hello.html\n");
    printf("  cat public/en/posts/hello.html\n");
    return 0;
}
