/*
 * linker.c - Entry Linker for Bilingual Support
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/cxo.h"

#define HASH_TABLE_SIZE 64

/* Simple hash table entry */
typedef struct hash_entry {
    char* id;
    cxo_entry_t* entry;
    struct hash_entry* next;
} hash_entry_t;

/* djb2 hash function */
static unsigned long hash_string(const char* str)
{
    unsigned long hash;
    int c;
    
    hash = 5381;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash % HASH_TABLE_SIZE;
}

/* Insert entry into hash table, returns existing entry if duplicate id */
static cxo_entry_t* hash_table_insert(hash_entry_t** table, arena_t* arena,
                                      const char* id, cxo_entry_t* entry)
{
    unsigned long hash;
    hash_entry_t* new_entry;
    hash_entry_t* current;
    
    if (!id) {
        return NULL;
    }
    
    hash = hash_string(id);
    
    /* Check for existing entry with same id */
    current = table[hash];
    while (current) {
        if (strcmp(current->id, id) == 0) {
            /* Found existing entry, return it for linking */
            return current->entry;
        }
        current = current->next;
    }
    
    /* Create new entry using arena */
    new_entry = arena_alloc(arena, sizeof(hash_entry_t));
    if (!new_entry) {
        return NULL;
    }
    
    new_entry->id = (char*)id;
    new_entry->entry = entry;
    new_entry->next = table[hash];
    table[hash] = new_entry;
    
    return NULL;
}

/* Link two entries as bilingual pair */
static void link_entry_pair(cxo_entry_t* a, cxo_entry_t* b, int* linked_count)
{
    if (strcmp(a->lang, b->lang) == 0) {
        fprintf(stderr, "Warning: duplicate id '%s' in same language '%s'\n",
                a->id, a->lang);
        return;
    }
    
    a->peer = b;
    b->peer = a;
    (*linked_count)++;
    
    printf("Linked: %s (%s) <-> %s (%s)\n",
           a->slug, a->lang, b->slug, b->lang);
}

/* Link entries by id - associate Chinese and English versions */
int cxo_link_entries(cxo_context_t* ctx, arena_t* arena)
{
    hash_entry_t* table[HASH_TABLE_SIZE] = {NULL};
    size_t i;
    int linked_count;
    
    linked_count = 0;
    
    /* Process all entries */
    for (i = 0; i < ctx->count; i++) {
        cxo_entry_t* entry = ctx->entries[i];
        cxo_entry_t* existing;
        
        if (!entry->id) {
            fprintf(stderr, "Warning: entry %zu has no id, skipping\n", i);
            continue;
        }
        
        /* Try to insert, if returns non-NULL, we found a match */
        existing = hash_table_insert(table, arena, entry->id, entry);
        if (!existing) {
            continue;
        }
        
        link_entry_pair(existing, entry, &linked_count);
    }
    
    printf("Linked %d bilingual entry pairs\n", linked_count);
    
    /* Table memory will be freed with arena, no cleanup needed */
    return 0;
}
