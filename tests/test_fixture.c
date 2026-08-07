/*
 * test_fixture.c - Fixture-based integration test
 *
 * Builds an independent mini-site from tests/fixtures/ in a temporary
 * directory (so assertions never depend on the repo's own content/)
 * and verifies the full pipeline output, including edge cases:
 * missing frontmatter, empty tags, auto excerpt, Chinese and code-span
 * headings, draft mode, attribute escaping, bilingual routing.
 *
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#ifdef _WIN32
#include <process.h>
#endif
#include "../include/cxo.h"
#include "../src/renderer_internal.h"

extern int cmd_build(void);

static int failures = 0;
static arena_t* g_arena;

#define CHECK(cond, name) do { \
    if (cond) { \
        printf("PASS: %s\n", name); \
    } else { \
        printf("FAIL: %s\n", name); \
        failures++; \
    } \
} while (0)

static int file_exists(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/* Read a file of any size into the arena (sized by stat, no cap) */
static int file_contains(const char* path, const char* needle)
{
    struct stat st;
    FILE* fp;
    char* buf;
    size_t n;
    int found;

    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return 0;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    buf = arena_alloc(g_arena, (size_t)st.st_size + 1);
    if (!buf) {
        fclose(fp);
        return 0;
    }
    n = fread(buf, 1, (size_t)st.st_size, fp);
    fclose(fp);
    buf[n] = '\0';
    found = strstr(buf, needle) != NULL;
    return found;
}

/* lstat where available so symlinks are never followed */
#ifdef _WIN32
#define test_lstat stat
#else
#define test_lstat lstat
#endif

/* Recursively delete a directory tree (does not follow symlinks) */
static void rm_tree(const char* path)
{
    DIR* dir;
    struct dirent* entry;
    struct stat st;
    char child[512];

    if (test_lstat(path, &st) != 0) {
        return;
    }
    if (!S_ISDIR(st.st_mode)) {
        remove(path);
        return;
    }

    dir = opendir(path);
    if (!dir) {
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        rm_tree(child);
    }
    closedir(dir);
    rmdir(path);
}

/* Copy fixtures into a fresh per-process work dir and chdir into it */
static int setup_workspace(char* work_dir, size_t size)
{
#ifdef _WIN32
    int pid = _getpid();
#else
    int pid = getpid();
#endif

    snprintf(work_dir, size, "tests/.fixture_work_%d", pid);
    rm_tree(work_dir);
    if (cxo_mkdir(work_dir) != 0) {
        return -1;
    }
    if (CXO_IS_ERR(copy_dir_recursive("tests/fixtures", work_dir))) {
        rm_tree(work_dir);
        return -1;
    }
    return chdir(work_dir);
}

int main(void)
{
    char work_dir[128];
    int rc;

    printf("Running fixture integration test...\n\n");

    g_arena = arena_create(1024 * 1024);
    if (!g_arena) {
        fprintf(stderr, "FAIL: cannot create arena\n");
        return 1;
    }

    if (setup_workspace(work_dir, sizeof(work_dir)) != 0) {
        fprintf(stderr, "FAIL: cannot set up fixture workspace\n");
        return 1;
    }

    rc = cmd_build();
    CHECK(rc == CXO_OK, "build succeeds");

    /* Core outputs */
    CHECK(file_exists("public/index.html"), "zh index exists");
    CHECK(file_exists("public/en/index.html"), "en index exists");
    CHECK(file_exists("public/posts/hello.html"), "zh post exists");
    CHECK(file_exists("public/en/posts/hello.html"), "en post exists");
    CHECK(file_exists("public/rss.xml"), "zh RSS exists");
    CHECK(file_exists("public/en/rss.xml"), "en RSS exists");
    CHECK(file_exists("public/sitemap.xml"), "sitemap exists");
    CHECK(file_exists("public/asset.txt"), "static asset copied");
    CHECK(file_exists("public/.well-known/asset.txt"),
          "static dotfile copied");

    /* Bilingual routing and linking */
    CHECK(file_contains("public/posts/hello.html",
                        "og:url\" content=\""
                        "https://fixture.example/posts/hello.html\""),
          "zh canonical og:url (no lang prefix)");
    CHECK(file_contains("public/en/posts/hello.html",
                        "https://fixture.example/en/posts/hello.html"),
          "en canonical og:url");
    CHECK(file_contains("public/posts/hello.html",
                        "href=\"/en/posts/hello.html\""),
          "lang switch links to en peer");

    /* TOC: AST-collected text, UTF-8 slugs, no inline tags in anchors */
    CHECK(file_contains("public/posts/hello.html", "id=\"安装指南\""),
          "Chinese heading anchor id");
    CHECK(file_contains("public/posts/hello.html",
                        "<a href=\"#使用-code-标题\">"
                        "使用 code 标题</a>"),
          "code-span heading: clean TOC text and slug");

    /* Escaping contract: quoted title escaped in text and meta */
    CHECK(file_contains("public/posts/hello.html",
                        "<title>你好 &quot;世界&quot;</title>"),
          "title escaped in <title>");
    CHECK(file_contains("public/posts/hello.html",
                        "og:title\" content=\"你好 &quot;世界&quot;\""),
          "title escaped in og:title");

    /* Edge cases */
    CHECK(file_exists("public/posts/bare.html"), "no-frontmatter post built");
    CHECK(file_contains("public/posts/bare.html", "1970-01-01"),
          "no-frontmatter post gets default date");
    CHECK(file_exists("public/en/posts/edge.html"), "empty-tags post built");
    CHECK(file_contains("public/en/posts/edge.html", "Auto excerpt body text"),
          "auto excerpt from content");
    CHECK(file_exists("public/tags/cxo.html"), "tag page exists");
    CHECK(!file_exists("public/posts/draft.html"),
          "draft excluded by default");

    /* Draft mode (env stays set; process exits right after) */
    cxo_setenv("CXO_DRAFT", "1");
    rc = cmd_build();
    CHECK(rc == CXO_OK, "draft-mode build succeeds");
    CHECK(file_exists("public/posts/draft.html"),
          "draft included with CXO_DRAFT=1");

    /* Hot reload injection contract: only when CXO_HOTRELOAD is set */
    CHECK(!file_contains("public/posts/hello.html", "__cxo_reload"),
          "hotreload script absent by default");
    cxo_setenv("CXO_HOTRELOAD", "1");
    rc = cmd_build();
    CHECK(rc == CXO_OK, "hotreload-mode build succeeds");
    CHECK(file_contains("public/posts/hello.html", "__cxo_reload"),
          "hotreload script injected with CXO_HOTRELOAD=1");

    /* Cleanup */
    if (chdir("../..") == 0) {
        rm_tree(work_dir);
    }
    arena_destroy(g_arena);

    printf("\n%s\n", failures == 0 ? "All fixture tests passed!"
                                   : "Some fixture tests FAILED");
    return failures == 0 ? 0 : 1;
}
