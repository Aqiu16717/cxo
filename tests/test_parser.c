/* Test parser module */
#include "include/arena.h"
#include "include/cxo.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    arena_t* arena;
    cxo_context_t* ctx;
    cxo_entry_t* entry;
    int ret;
    
    printf("Testing parser...\n\n");
    
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
    
    /* Scan first */
    ret = cxo_scan_content(ctx, arena, "content");
    if (CXO_IS_ERR(ret) || ctx->count == 0) {
        printf("FAIL: no content found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("PASS: scanned %lu entries\n", (unsigned long)ctx->count);
    
    /* Find hello entry for detailed test */
    entry = NULL;
    for (size_t i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->entries[i]->slug, "hello") == 0) {
            entry = ctx->entries[i];
            break;
        }
    }
    if (!entry) {
        printf("FAIL: hello entry not found\n");
        arena_destroy(arena);
        return 1;
    }
    printf("\nParsing entry: %s (%s)\n", entry->slug, entry->lang);
    
    ret = cxo_parse_markdown(entry, arena, NULL);
    if (CXO_IS_ERR(ret)) {
        printf("FAIL: cxo_parse_markdown returned %d\n", ret);
        arena_destroy(arena);
        return 1;
    }
    printf("PASS: cxo_parse_markdown\n");
    
    /* Verify frontmatter was parsed */
    printf("\nParsed frontmatter:\n");
    printf("  id: %s\n", entry->id ? entry->id : "(null)");
    printf("  title: %s\n", entry->title ? entry->title : "(null)");
    printf("  date: %s\n", entry->date ? entry->date : "(null)");
    printf("  slug: %s\n", entry->slug ? entry->slug : "(null)");
    printf("  description: %s\n",
           entry->description ? entry->description : "(null)");
    printf("  tags: %lu\n", (unsigned long)entry->tag_count);
    
    /* Verify description and tags */
    if (!entry->description || strlen(entry->description) == 0) {
        printf("FAIL: description is missing\n");
        arena_destroy(arena);
        return 1;
    }
    printf("PASS: description present\n");
    
    if (entry->tag_count != 2) {
        printf("FAIL: expected 2 tags, got %lu\n", (unsigned long)entry->tag_count);
        arena_destroy(arena);
        return 1;
    }
    printf("PASS: tag count is 2 (%s, %s)\n", entry->tags[0], entry->tags[1]);
    
    /* Verify HTML content was generated */
    if (!entry->html_content) {
        printf("FAIL: html_content is NULL\n");
        arena_destroy(arena);
        return 1;
    }
    
    printf("\nGenerated HTML (first 200 chars):\n");
    printf("%.200s...\n", entry->html_content);
    
    /* Check that HTML contains expected tags */
    if (strstr(entry->html_content, "<p>") == NULL) {
        printf("FAIL: HTML doesn't contain <p> tag\n");
        arena_destroy(arena);
        return 1;
    }
    printf("\nPASS: HTML contains expected tags\n");
    
    /* Parse all entries */
    printf("\nParsing all entries...\n");
    for (size_t i = 0; i < ctx->count; i++) {
        entry = ctx->entries[i];
        ret = cxo_parse_markdown(entry, arena, NULL);
        if (CXO_IS_ERR(ret)) {
            printf("  FAIL: entry %lu (%s)\n", (unsigned long)i, entry->slug);
        } else {
            printf("  OK: %s - \"%s\"\n", entry->slug, entry->title);
        }
    }
    
    arena_destroy(arena);
    printf("\nPASS: cleanup\n");
    printf("\nAll parser tests passed!\n");
    return 0;
}
