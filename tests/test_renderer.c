/* Test renderer module */
#include "include/arena.h"
#include "include/cxo.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
    printf("PASS: scanned %lu entries\n", (unsigned long)ctx->count);
    
    /* Parse */
    for (size_t i = 0; i < ctx->count; i++) {
        rc = cxo_parse_markdown(ctx->entries[i], arena, NULL);
        if (CXO_IS_ERR(rc)) {
            printf("FAIL: parse entry %lu\n", (unsigned long)i);
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
    
    /* Create test static asset */
    {
        FILE* fp;
        cxo_mkdir("static");
        fp = fopen("static/test_asset.txt", "w");
        if (fp) {
            fprintf(fp, "static asset test\n");
            fclose(fp);
        }
    }
    
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
    
    if (stat("public/posts/second.html", &st) != 0) {
        printf("FAIL: public/posts/second.html not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/posts/second.html\n");
    
    if (stat("public/en/posts/second.html", &st) != 0) {
        printf("FAIL: public/en/posts/second.html not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/en/posts/second.html\n");
    
    /* Verify tag pages exist */
    if (stat("public/tags/cxo.html", &st) != 0) {
        printf("FAIL: public/tags/cxo.html not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/tags/cxo.html\n");
    
    if (stat("public/tags/welcome.html", &st) != 0) {
        printf("FAIL: public/tags/welcome.html not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/tags/welcome.html\n");
    
    if (stat("public/tags/next.html", &st) != 0) {
        printf("FAIL: public/tags/next.html not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/tags/next.html\n");
    
    /* Verify archive pages exist */
    if (stat("public/2026/index.html", &st) != 0) {
        printf("FAIL: public/2026/index.html not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/2026/index.html\n");
    
    if (stat("public/2026/03/index.html", &st) != 0) {
        printf("FAIL: public/2026/03/index.html not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/2026/03/index.html\n");
    
    if (stat("public/en/2026/index.html", &st) != 0) {
        printf("FAIL: public/en/2026/index.html not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/en/2026/index.html\n");
    
    if (stat("public/en/2026/03/index.html", &st) != 0) {
        printf("FAIL: public/en/2026/03/index.html not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/en/2026/03/index.html\n");
    
    /* Verify archive page content */
    {
        FILE* fp;
        char buf[4096];
        size_t n;
        
        fp = fopen("public/2026/index.html", "r");
        if (!fp) {
            printf("FAIL: cannot read public/2026/index.html\n");
            arena_destroy(arena);
            return 1;
        }
        n = fread(buf, 1, sizeof(buf) - 1, fp);
        buf[n] = '\0';
        fclose(fp);
        
        if (strstr(buf, "2026") == NULL) {
            printf("FAIL: archive title missing in 2026/index.html\n");
            arena_destroy(arena);
            return 1;
        }
        printf("  Found archive title in 2026/index.html\n");
        
        if (strstr(buf, "Second Post") == NULL) {
            printf("FAIL: archive missing Second Post\n");
            arena_destroy(arena);
            return 1;
        }
        printf("  Found post link in archive page\n");
    }
    
    /* Verify prev/next navigation */
    {
        FILE* fp;
        char buf[4096];
        size_t n;
        
        fp = fopen("public/posts/second.html", "r");
        if (!fp) {
            printf("FAIL: cannot read public/posts/second.html\n");
            arena_destroy(arena);
            return 1;
        }
        n = fread(buf, 1, sizeof(buf) - 1, fp);
        buf[n] = '\0';
        fclose(fp);
        
        if (strstr(buf, "class=\"prev\"") == NULL) {
            printf("FAIL: missing prev nav in second.html\n");
            arena_destroy(arena);
            return 1;
        }
        printf("  Found prev nav in second.html\n");
        
        if (strstr(buf, "Hello World") == NULL) {
            printf("FAIL: prev link target not found in second.html\n");
            arena_destroy(arena);
            return 1;
        }
        printf("  Found prev link target (Hello World)\n");
    }
    
    {
        FILE* fp;
        char buf[4096];
        size_t n;
        
        fp = fopen("public/posts/hello.html", "r");
        if (!fp) {
            printf("FAIL: cannot read public/posts/hello.html\n");
            arena_destroy(arena);
            return 1;
        }
        n = fread(buf, 1, sizeof(buf) - 1, fp);
        buf[n] = '\0';
        fclose(fp);
        
        if (strstr(buf, "class=\"next\"") == NULL) {
            printf("FAIL: missing next nav in hello.html\n");
            arena_destroy(arena);
            return 1;
        }
        printf("  Found next nav in hello.html\n");
    }
    
    /* Verify static files copied */
    if (stat("public/test_asset.txt", &st) != 0) {
        printf("FAIL: public/test_asset.txt not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("  Found: public/test_asset.txt\n");
    
    {
        FILE* fp;
        char buf[64];
        size_t n;
        
        fp = fopen("public/test_asset.txt", "r");
        if (!fp) {
            printf("FAIL: cannot read public/test_asset.txt\n");
            arena_destroy(arena);
            return 1;
        }
        n = fread(buf, 1, sizeof(buf) - 1, fp);
        buf[n] = '\0';
        fclose(fp);
        
        if (strstr(buf, "static asset test") == NULL) {
            printf("FAIL: static file content mismatch\n");
            arena_destroy(arena);
            return 1;
        }
        printf("  Static file content correct\n");
    }
    
    /* Cleanup test static asset */
    remove("static/test_asset.txt");
    rmdir("static");
    remove("public/test_asset.txt");
    
    arena_destroy(arena);
    printf("\nPASS: cleanup\n");
    printf("\nAll renderer tests passed!\n");
    printf("\nView generated files:\n");
    printf("  cat public/posts/hello.html\n");
    printf("  cat public/en/posts/hello.html\n");
    return 0;
}
