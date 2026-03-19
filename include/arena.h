/*
 * arena.h - A simple arena (region-based) memory allocator in pure C
 *
 * This is a single-header library. To use it, define ARENA_IMPLEMENTATION
 * before including this file in exactly one source file.
 *
 * Example:
 *   #define ARENA_IMPLEMENTATION
 *   #include "arena.h"
 *
 *   arena_t* a = arena_create(4096);  // Create arena with 4KB initial size
 *   void* ptr = arena_alloc(a, 256);  // Allocate 256 bytes
 *   arena_reset(a);                   // Reset arena (reuse memory)
 *   arena_destroy(a);                 // Free all memory
 */

#ifndef ARENA_H
#define ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque arena structure */
typedef struct arena arena_t;

/* ============================================================================
 * Public API
 * ============================================================================
 */

/* Create a new arena with the specified initial chunk size (in bytes) */
arena_t* arena_create(size_t chunk_size);

/* Destroy the arena and free all associated memory */
void arena_destroy(arena_t* a);

/* Allocate `size` bytes from the arena (uninitialized memory) */
void* arena_alloc(arena_t* a, size_t size);

/* Allocate `size` bytes from the arena, zero-initialized */
void* arena_calloc(arena_t* a, size_t size);

/* Allocate `count` elements of `size` bytes, zero-initialized */
void* arena_calloc_count(arena_t* a, size_t count, size_t size);

/* Reset the arena, freeing all allocations but keeping memory for reuse */
void arena_reset(arena_t* a);

/* Get total bytes allocated (including overhead) */
size_t arena_total_allocated(const arena_t* a);

/* Get used bytes in current chunks */
size_t arena_used(const arena_t* a);

/* ============================================================================
 * Extended API - Save/Restore state
 * ============================================================================
 */

/* Opaque - implementation details */
typedef struct arena_state {
  void* internal[2];
} arena_state_t;

/* Save current arena position for later restoration */
arena_state_t arena_save(arena_t* a);

/* Restore arena to a previously saved position (frees allocations after that
 * point) */
void arena_restore(arena_t* a, arena_state_t state);

/* ============================================================================
 * Macros
 * ============================================================================
 */

/* Allocate space for a single object of type T */
#define arena_alloc_type(a, T) ((T *)arena_alloc((a), sizeof(T)))

/* Allocate array of N elements of type T */
#define arena_alloc_array(a, T, N)                                             \
  ((T *)arena_calloc_count((a), (N), sizeof(T)))

/* Allocate zeroed array of N elements of type T */
#define arena_calloc_array(a, T, N)                                            \
  ((T *)arena_calloc_count((a), (N), sizeof(T)))

/* Allocate a null-terminated string and copy s into it */
char* arena_strdup(arena_t* a, const char* s);

/* Allocate a null-terminated string with at most n characters from s */
char* arena_strndup(arena_t* a, const char* s, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* ARENA_H */

/* ============================================================================
 * Implementation
 * ============================================================================
 */

#ifdef ARENA_IMPLEMENTATION

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef ARENA_ALIGNMENT
/* Default alignment: 16 bytes on 64-bit, 8 bytes on 32-bit */
#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
#define ARENA_ALIGNMENT 16
#else
#define ARENA_ALIGNMENT 8
#endif
#endif

#ifndef ARENA_MIN_CHUNK_SIZE
#define ARENA_MIN_CHUNK_SIZE 4096
#endif

/* Memory region (chunk) structure */
typedef struct arena_region arena_region_t;
struct arena_region {
  arena_region_t* next;
  /* Total size of this region */
  size_t capacity;
  /* Bytes used in this region, data follows this structure */
  size_t used;
};

struct arena {
  /* Current region for allocations */
  arena_region_t* current;
  /* First region (for cleanup) */
  arena_region_t* first;
  /* Default size for new regions */
  size_t chunk_size;
  /* Total bytes allocated across all regions */
  size_t total;
};

/* Align size up to ARENA_ALIGNMENT */
static inline size_t arena_align_up(size_t size) {
  size_t mask = ARENA_ALIGNMENT - 1;
  if (size > SIZE_MAX - mask) {
    /* Overflow would occur */
    return 0;
  }
  return (size + mask) & ~mask;
}

/* Get aligned size of region header */
static inline size_t arena_header_size(void) {
  return arena_align_up(sizeof(arena_region_t));
}

/* Create a new region */
static arena_region_t* arena_region_create(size_t capacity) {
  size_t total_size = arena_header_size() + capacity;
  arena_region_t* r = (arena_region_t *)malloc(total_size);
  if (!r) {
    return NULL;
  }

  r->next = NULL;
  r->capacity = capacity;
  r->used = 0;
  return r;
}

/* Get pointer to data area of region */
static inline char* arena_region_data(arena_region_t* r) {
  return (char *)r + arena_header_size();
}

arena_t* arena_create(size_t chunk_size) {
  arena_t* a = (arena_t *)malloc(sizeof(arena_t));
  if (!a) {
    return NULL;
  }

  /* Ensure minimum chunk size */
  if (chunk_size < ARENA_MIN_CHUNK_SIZE) {
    chunk_size = ARENA_MIN_CHUNK_SIZE;
  }

  a->chunk_size = arena_align_up(chunk_size);
  a->total = 0;

  /* Create initial region */
  arena_region_t* r = arena_region_create(a->chunk_size);
  if (!r) {
    free(a);
    return NULL;
  }

  a->current = r;
  a->first = r;
  a->total = arena_header_size() + r->capacity;

  return a;
}

void arena_destroy(arena_t* a) {
  if (!a) {
    return;
  }

  arena_region_t* r = a->first;
  while (r) {
    arena_region_t* next = r->next;
    free(r);
    r = next;
  }

  free(a);
}

void* arena_alloc(arena_t* a, size_t size) {
  if (!a || size == 0) {
    return NULL;
  }

  size = arena_align_up(size);
  if (size == 0) {
    /* Overflow in alignment */
    return NULL;
  }

  /* Try to allocate from current region */
  arena_region_t* r = a->current;

  if (r->used + size <= r->capacity) {
    void* ptr = arena_region_data(r) + r->used;
    r->used += size;
    return ptr;
  }

  /* Need a new region */
  size_t new_capacity = size > a->chunk_size ? size : a->chunk_size;
  arena_region_t* new_region = arena_region_create(new_capacity);
  if (!new_region) {
    return NULL;
  }

  a->total += arena_header_size() + new_capacity;

  r->next = new_region;
  a->current = new_region;

  void* ptr = arena_region_data(new_region);
  new_region->used = size;
  return ptr;
}

void* arena_calloc(arena_t* a, size_t size) {
  void* ptr = arena_alloc(a, size);
  if (ptr) {
    memset(ptr, 0, size);
  }
  return ptr;
}

void* arena_calloc_count(arena_t* a, size_t count, size_t size) {
  /* Check for overflow */
  if (count == 0 || size == 0) {
    return NULL;
  }

  size_t total = count * size;
  if (total / count != size) {
    /* Overflow check */
    return NULL;
  }

  return arena_calloc(a, total);
}

void arena_reset(arena_t* a) {
  if (!a) {
    return;
  }

  arena_region_t* r = a->first;
  while (r) {
    r->used = 0;
    r = r->next;
  }

  a->current = a->first;
  a->total = arena_header_size() + a->first->capacity;

  /* Free extra regions, keep only the first one */
  r = a->first->next;
  while (r) {
    arena_region_t* next = r->next;
    free(r);
    r = next;
  }
  a->first->next = NULL;
}

size_t arena_total_allocated(const arena_t* a) {
  if (!a) {
    return 0;
  }
  return a->total;
}

size_t arena_used(const arena_t* a) {
  if (!a) {
    return 0;
  }

  size_t used = 0;
  arena_region_t* r = a->first;
  while (r) {
    used += r->used;
    r = r->next;
  }
  return used;
}

char* arena_strdup(arena_t* a, const char* s) {
  if (!s) {
    return NULL;
  }

  size_t len = strlen(s);
  char* copy = (char *)arena_alloc(a, len + 1);
  if (copy) {
    memcpy(copy, s, len + 1);
  }
  return copy;
}

char* arena_strndup(arena_t* a, const char* s, size_t n) {
  if (!s) {
    return NULL;
  }

  size_t len = strlen(s);
  if (len > n) {
    len = n;
  }

  char* copy = (char *)arena_alloc(a, len + 1);
  if (copy) {
    memcpy(copy, s, len);
    copy[len] = '\0';
  }
  return copy;
}

/* Save/Restore implementation */
arena_state_t arena_save(arena_t* a) {
  arena_state_t state = {{NULL, NULL}};
  if (!a) {
    return state;
  }
  state.internal[0] = a->current;
  state.internal[1] = (void *)((uintptr_t)a->current->used);
  return state;
}

void arena_restore(arena_t* a, arena_state_t state) {
  arena_region_t* saved_region;
  size_t saved_used;

  if (!a) {
    return;
  }

  saved_region = (arena_region_t *)state.internal[0];
  saved_used = (size_t)(uintptr_t)state.internal[1];

  /* Validate state belongs to this arena */
  if (!saved_region) {
    return;
  }

  /* Reset current region */
  a->current = saved_region;
  saved_region->used = saved_used;

  /* Free regions after saved position */
  arena_region_t* r = saved_region->next;
  while (r) {
    arena_region_t* next = r->next;
    free(r);
    r = next;
  }
  saved_region->next = NULL;
}

#endif /* ARENA_IMPLEMENTATION */
