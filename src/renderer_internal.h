/*
 * renderer_internal.h - Internal declarations shared by renderer modules
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#ifndef CXO_RENDERER_INTERNAL_H
#define CXO_RENDERER_INTERNAL_H

#include "../include/cxo.h"

/* Shared constants */
#define MAX_OUTPUT_PATH 4096
#define MAX_TEMPLATE_SIZE (256 * 1024)
#define MAX_ARCHIVE_PERIODS 128
#define MAX_UNIQUE_TAGS 256

/* path_util.c */
char* read_file_to_arena(arena_t* arena, const char* path);
int ensure_dir(const char* path);
int copy_static_files(const char* output_dir);
int copy_theme_assets(const char* theme_path, const char* output_dir);
const char* get_output_subdir(const char* lang);

/* template.c */
extern const char* hotreload_script;

/* Template variable for single-pass substitution */
typedef struct {
    const char* key;
    const char* value;
} cxo_var_t;

char* replace_vars(arena_t* arena, const char* tmpl,
                   const cxo_var_t* vars, size_t var_count);
char* escape_attr(arena_t* arena, const char* src);
char* load_template(arena_t* arena, const char* theme_path);
char* load_tag_template(arena_t* arena, const char* theme_path);
char* load_index_template(arena_t* arena, const char* theme_path);
char* load_archive_template(arena_t* arena, const char* theme_path);
int hotreload_enabled(void);
int show_drafts(void);

/* render_posts.c */
void sort_entries(cxo_context_t* ctx);
void assign_prev_next(cxo_context_t* ctx);
char* build_site_meta_tags(const cxo_context_t* ctx, arena_t* arena,
                           const char* lang, const char* page_url);
int write_html(const char* path, const char* html);
int render_entry(cxo_entry_t* entry, cxo_context_t* ctx, arena_t* arena,
                 const char* output_dir, const char* tmpl);

/* render_index.c */
size_t count_matching_entries(cxo_context_t* ctx, const char* lang,
                              int include_drafts);
int render_index(cxo_context_t* ctx, arena_t* arena, const char* output_dir,
                 const char* lang, const char* tmpl);

/* render_taxonomy.c */
size_t collect_unique_tags(cxo_context_t* ctx, char** tags_out);
int render_tag_page(cxo_context_t* ctx, arena_t* arena,
                    const char* output_dir, const char* lang,
                    const char* tag, const char* tmpl);
int render_archive_pages(cxo_context_t* ctx, arena_t* arena,
                         const char* output_dir, const char* lang,
                         const char* tmpl);

/* render_feeds.c */
int render_rss(cxo_context_t* ctx, arena_t* arena, const char* output_dir,
               const char* lang);
int render_sitemap(cxo_context_t* ctx, arena_t* arena,
                   const char* output_dir);

#endif /* CXO_RENDERER_INTERNAL_H */
