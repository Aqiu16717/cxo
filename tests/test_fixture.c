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
#include "../include/cxo.h"

extern int cmd_build(void);

static int failures = 0;

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

/* Read whole file into a static buffer (fixture outputs are small) */
static char* read_whole(const char* path)
{
    static char buf[512 * 1024];
    FILE* fp;
    size_t n;

    fp = fopen(path, "rb");
    if (!fp) {
        return NULL;
    }
    n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    return buf;
}

static int file_contains(const char* path, const char* needle)
{
    char* content = read_whole(path);
    return content && strstr(content, needle) != NULL;
}

/* Copy fixtures into a fresh temp dir and chdir into it */
static int setup_workspace(char* tmpdir, size_t size)
{
    char cmd[1024];
    char cwd[512];

    strcpy(tmpdir, "/tmp/cxo_fixture_XXXXXX");
    if (!mkdtemp(tmpdir)) {
        return -1;
    }
    if (size < strlen(tmpdir) + 1) {
        return -1;
    }

    if (!getcwd(cwd, sizeof(cwd))) {
        return -1;
    }
    snprintf(cmd, sizeof(cmd), "cp -r '%s/tests/fixtures'/. '%s'/",
             cwd, tmpdir);
    if (system(cmd) != 0) {
        return -1;
    }
    return chdir(tmpdir);
}

int main(void)
{
    char tmpdir[512];
    char cleanup[600];
    int rc;

    printf("Running fixture integration test...\n\n");

    if (setup_workspace(tmpdir, sizeof(tmpdir)) != 0) {
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

    /* Bilingual routing and linking */
    CHECK(file_contains("public/posts/hello.html",
                        "og:url\" content=\"https://fixture.example/posts/hello.html\""),
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
                        "<a href=\"#使用-code-标题\">使用 code 标题</a>"),
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

    /* Draft mode */
    setenv("CXO_DRAFT", "1", 1);
    rc = cmd_build();
    unsetenv("CXO_DRAFT");
    CHECK(rc == CXO_OK, "draft-mode build succeeds");
    CHECK(file_exists("public/posts/draft.html"),
          "draft included with CXO_DRAFT=1");

    /* Cleanup */
    chdir("/");
    snprintf(cleanup, sizeof(cleanup), "rm -rf '%s'", tmpdir);
    if (system(cleanup) != 0) {
        fprintf(stderr, "Warning: failed to remove %s\n", tmpdir);
    }

    printf("\n%s\n", failures == 0 ? "All fixture tests passed!"
                                   : "Some fixture tests FAILED");
    return failures == 0 ? 0 : 1;
}
