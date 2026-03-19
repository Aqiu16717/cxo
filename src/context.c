/*
 * context.c - Context and Entry Management
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <string.h>
#include "../include/cxo.h"

cxo_context_t* cxo_context_create(arena_t* arena)
{
    cxo_context_t* ctx;
    
    ctx = arena_calloc(arena, sizeof(cxo_context_t));
    if (!ctx) {
        return NULL;
    }
    
    return ctx;
}

cxo_entry_t* cxo_entry_create(arena_t* arena)
{
    cxo_entry_t* entry;
    
    entry = arena_calloc(arena, sizeof(cxo_entry_t));
    if (!entry) {
        return NULL;
    }
    
    return entry;
}
