/* Test config module */
#include "include/arena.h"
#include "include/cxo.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    arena_t* arena;
    cxo_context_t* ctx;
    int rc;
    
    printf("Testing config...\n\n");
    
    /* Setup */
    arena = arena_create(4096);
    ctx = cxo_context_create(arena);
    if (!ctx || !arena) {
        printf("FAIL: setup\n");
        return 1;
    }
    printf("PASS: setup\n");
    
    /* Test loading with defaults (no file) */
    rc = cxo_load_config(ctx, arena, "nonexistent.toml");
    if (CXO_IS_ERR(rc)) {
        printf("FAIL: should succeed with defaults\n");
        return 1;
    }
    printf("PASS: load with defaults (no file)\n");
    printf("  title: %s\n", ctx->site_title);
    printf("  base_url: %s\n", ctx->base_url);
    
    /* Test loading from config.toml */
    rc = cxo_load_config(ctx, arena, "config.toml");
    if (CXO_IS_ERR(rc)) {
        printf("FAIL: load config.toml\n");
        return 1;
    }
    printf("PASS: load config.toml\n");
    printf("  title: %s\n", ctx->site_title);
    printf("  description: %s\n", ctx->site_description);
    printf("  base_url: %s\n", ctx->base_url);
    printf("  theme: %s\n", ctx->theme_path);
    
    /* Verify values from file */
    if (strcmp(ctx->site_title, "My Blog") != 0) {
        printf("FAIL: title mismatch\n");
        return 1;
    }
    
    arena_destroy(arena);
    printf("\nAll config tests passed!\n");
    return 0;
}
