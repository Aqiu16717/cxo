/*
 * cmd_init.c - Initialize new CXO project
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "../include/platform.h"
#include "../include/cxo.h"

/* Default config.toml template */
static const char* config_template =
    "[site]\n"
    "title = \"My Blog\"\n"
    "description = \"A minimalist blog powered by CXO\"\n"
    "base_url = \"https://example.com\"\n"
    "posts_per_page = 10\n"
    "\n"
    "[theme]\n"
    "path = \"themes/default\"\n";

/* Default CSS */
static const char* css_template =
    ":root {\n"
    "  --bg: #fff;\n"
    "  --fg: #333;\n"
    "  --muted: #555;\n"
    "  --border: #e9ecef;\n"
    "  --toc-bg: #f8f9fa;\n"
    "  --toc-border: #e9ecef;\n"
    "  --toc-link: #555;\n"
    "  --toc-hover: #000;\n"
    "  --pagination-border: #ddd;\n"
    "  --pagination-hover: #f5f5f5;\n"
    "}\n"
    "\n"
    "[data-theme=\"dark\"] {\n"
    "  --bg: #1a1a1a;\n"
    "  --fg: #e0e0e0;\n"
    "  --muted: #aaa;\n"
    "  --border: #444;\n"
    "  --toc-bg: #252525;\n"
    "  --toc-border: #444;\n"
    "  --toc-link: #aaa;\n"
    "  --toc-hover: #fff;\n"
    "  --pagination-border: #555;\n"
    "  --pagination-hover: #333;\n"
    "  color-scheme: dark;\n"
    "}\n"
    "\n"
    "body {\n"
    "  font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Helvetica, Arial, sans-serif;\n"
    "  max-width: 800px;\n"
    "  margin: 40px auto;\n"
    "  padding: 0 20px;\n"
    "  line-height: 1.6;\n"
    "  color: var(--fg);\n"
    "  background: var(--bg);\n"
    "}\n"
    "\n"
    "/* Table of contents */\n"
    ".toc {\n"
    "  background: var(--toc-bg);\n"
    "  border: 1px solid var(--toc-border);\n"
    "  border-radius: 4px;\n"
    "  padding: 16px 20px;\n"
    "  margin-bottom: 24px;\n"
    "}\n"
    "\n"
    ".toc ul {\n"
    "  margin: 0;\n"
    "  padding-left: 20px;\n"
    "}\n"
    "\n"
    ".toc li {\n"
    "  margin: 4px 0;\n"
    "}\n"
    "\n"
    ".toc a {\n"
    "  color: var(--toc-link);\n"
    "  text-decoration: none;\n"
    "}\n"
    "\n"
    ".toc a:hover {\n"
    "  color: var(--toc-hover);\n"
    "  text-decoration: underline;\n"
    "}\n"
    "\n"
    "/* Pagination */\n"
    ".pagination {\n"
    "  margin-top: 32px;\n"
    "  text-align: center;\n"
    "}\n"
    "\n"
    ".pagination a {\n"
    "  display: inline-block;\n"
    "  margin: 0 8px;\n"
    "  padding: 8px 16px;\n"
    "  border: 1px solid var(--pagination-border);\n"
    "  border-radius: 4px;\n"
    "  color: var(--fg);\n"
    "  text-decoration: none;\n"
    "}\n"
    "\n"
    ".pagination a:hover {\n"
    "  background: var(--pagination-hover);\n"
    "}\n"
    "\n"
    "/* Theme toggle */\n"
    "nav {\n"
    "  overflow: hidden;\n"
    "  margin-bottom: 20px;\n"
    "}\n"
    "\n"
    "nav a {\n"
    "  color: var(--fg);\n"
    "  text-decoration: none;\n"
    "  font-weight: bold;\n"
    "}\n"
    "\n"
    ".theme-toggle {\n"
    "  float: right;\n"
    "  background: transparent;\n"
    "  border: 1px solid var(--pagination-border);\n"
    "  border-radius: 4px;\n"
    "  padding: 4px 12px;\n"
    "  cursor: pointer;\n"
    "  color: var(--fg);\n"
    "  font-size: 14px;\n"
    "}\n"
    "\n"
    ".theme-toggle:hover {\n"
    "  background: var(--pagination-hover);\n"
    "}\n";

/* Default index template */
static const char* index_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<title>{{site_title}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var s = localStorage.getItem('cxo-theme');\n"
    "  var d = window.matchMedia('(prefers-color-scheme: dark)').matches;\n"
    "  if (s === 'dark' || (!s && d)) h.setAttribute('data-theme', 'dark');\n"
    "})();\n"
    "</script>\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a></nav>\n"
    "<h1>{{site_title}}</h1>\n"
    "<ul class=\"post-list\">\n"
    "{{entry_list}}"
    "</ul>\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var b = document.createElement('button');\n"
    "  b.className = 'theme-toggle';\n"
    "  b.textContent = h.getAttribute('data-theme') === 'dark' ? '☀️' : '🌙';\n"
    "  b.onclick = function() {\n"
    "    if (h.getAttribute('data-theme') === 'dark') {\n"
    "      h.removeAttribute('data-theme');\n"
    "      localStorage.setItem('cxo-theme', 'light');\n"
    "      b.textContent = '🌙';\n"
    "    } else {\n"
    "      h.setAttribute('data-theme', 'dark');\n"
    "      localStorage.setItem('cxo-theme', 'dark');\n"
    "      b.textContent = '☀️';\n"
    "    }\n"
    "  };\n"
    "  var n = document.querySelector('nav');\n"
    "  if (n) n.appendChild(b);\n"
    "})();\n"
    "</script>\n"
    "</body>\n"
    "</html>\n";

/* Default HTML template */
static const char* html_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<title>{{title}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var s = localStorage.getItem('cxo-theme');\n"
    "  var d = window.matchMedia('(prefers-color-scheme: dark)').matches;\n"
    "  if (s === 'dark' || (!s && d)) h.setAttribute('data-theme', 'dark');\n"
    "})();\n"
    "</script>\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a></nav>\n"
    "<article>\n"
    "<h1>{{title}}</h1>\n"
    "<div class=\"content\">{{content}}</div>\n"
    "</article>\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var b = document.createElement('button');\n"
    "  b.className = 'theme-toggle';\n"
    "  b.textContent = h.getAttribute('data-theme') === 'dark' ? '☀️' : '🌙';\n"
    "  b.onclick = function() {\n"
    "    if (h.getAttribute('data-theme') === 'dark') {\n"
    "      h.removeAttribute('data-theme');\n"
    "      localStorage.setItem('cxo-theme', 'light');\n"
    "      b.textContent = '🌙';\n"
    "    } else {\n"
    "      h.setAttribute('data-theme', 'dark');\n"
    "      localStorage.setItem('cxo-theme', 'dark');\n"
    "      b.textContent = '☀️';\n"
    "    }\n"
    "  };\n"
    "  var n = document.querySelector('nav');\n"
    "  if (n) n.appendChild(b);\n"
    "})();\n"
    "</script>\n"
    "</body>\n"
    "</html>\n";

/* Create directory recursively */
static int mkdir_p(const char* path)
{
    struct stat st;
    char tmp[256];
    char* p;
    
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? CXO_OK : CXO_ERR_IO;
    }
    
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            cxo_mkdir(tmp);
            *p = '/';
        }
    }
    
    if (cxo_mkdir(tmp) != 0 && errno != EEXIST) {
        return CXO_ERR_IO;
    }
    
    return CXO_OK;
}

/* Write string to file */
static int write_file(const char* path, const char* content)
{
    FILE* fp;
    
    fp = fopen(path, "w");
    if (!fp) {
        return CXO_ERR_IO;
    }
    
    fprintf(fp, "%s", content);
    fclose(fp);
    return CXO_OK;
}

/* Get current date */
static void get_current_date(char* buf, size_t size)
{
    time_t now;
    struct tm* tm_info;
    
    time(&now);
    tm_info = localtime(&now);
    strftime(buf, size, "%Y-%m-%d", tm_info);
}

/* Create project directories */
static int create_project_dirs(void)
{
    size_t i;
    char path[64];

    for (i = 0; i < CXO_LANG_COUNT; i++) {
        snprintf(path, sizeof(path), "content/%s", CXO_LANGS[i].code);
        mkdir_p(path);
    }
    mkdir_p("themes/default");
    mkdir_p("static");
    return CXO_OK;
}

/* Create sample post */
static int create_sample_post(void)
{
    char date[16];
    char path[64];
    char post_content[512];

    get_current_date(date, sizeof(date));
    snprintf(post_content, sizeof(post_content),
             "---\n"
             "id: hello\n"
             "title: Hello World\n"
             "date: %s\n"
             "---\n"
             "\n"
             "Welcome to CXO!\n", date);

    snprintf(path, sizeof(path), "content/%s/hello.md", CXO_LANGS[0].code);
    write_file(path, post_content);
    printf("Created: %s\n", path);
    return CXO_OK;
}

/* Default tag template */
static const char* tag_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<title>{{site_title}} - {{tag_name}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var s = localStorage.getItem('cxo-theme');\n"
    "  var d = window.matchMedia('(prefers-color-scheme: dark)').matches;\n"
    "  if (s === 'dark' || (!s && d)) h.setAttribute('data-theme', 'dark');\n"
    "})();\n"
    "</script>\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a></nav>\n"
    "<h1>{{tag_name}}</h1>\n"
    "<ul class=\"post-list\">\n"
    "{{entry_list}}"
    "</ul>\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var b = document.createElement('button');\n"
    "  b.className = 'theme-toggle';\n"
    "  b.textContent = h.getAttribute('data-theme') === 'dark' ? '☀️' : '🌙';\n"
    "  b.onclick = function() {\n"
    "    if (h.getAttribute('data-theme') === 'dark') {\n"
    "      h.removeAttribute('data-theme');\n"
    "      localStorage.setItem('cxo-theme', 'light');\n"
    "      b.textContent = '🌙';\n"
    "    } else {\n"
    "      h.setAttribute('data-theme', 'dark');\n"
    "      localStorage.setItem('cxo-theme', 'dark');\n"
    "      b.textContent = '☀️';\n"
    "    }\n"
    "  };\n"
    "  var n = document.querySelector('nav');\n"
    "  if (n) n.appendChild(b);\n"
    "})();\n"
    "</script>\n"
    "</body>\n"
    "</html>\n";

/* Default archive template */
static const char* archive_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<title>{{site_title}} - {{archive_title}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var s = localStorage.getItem('cxo-theme');\n"
    "  var d = window.matchMedia('(prefers-color-scheme: dark)').matches;\n"
    "  if (s === 'dark' || (!s && d)) h.setAttribute('data-theme', 'dark');\n"
    "})();\n"
    "</script>\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a></nav>\n"
    "<h1>{{archive_title}}</h1>\n"
    "<ul class=\"post-list\">\n"
    "{{entry_list}}"
    "</ul>\n"
    "<script>\n"
    "(function() {\n"
    "  var h = document.documentElement;\n"
    "  var b = document.createElement('button');\n"
    "  b.className = 'theme-toggle';\n"
    "  b.textContent = h.getAttribute('data-theme') === 'dark' ? '☀️' : '🌙';\n"
    "  b.onclick = function() {\n"
    "    if (h.getAttribute('data-theme') === 'dark') {\n"
    "      h.removeAttribute('data-theme');\n"
    "      localStorage.setItem('cxo-theme', 'light');\n"
    "      b.textContent = '🌙';\n"
    "    } else {\n"
    "      h.setAttribute('data-theme', 'dark');\n"
    "      localStorage.setItem('cxo-theme', 'dark');\n"
    "      b.textContent = '☀️';\n"
    "    }\n"
    "  };\n"
    "  var n = document.querySelector('nav');\n"
    "  if (n) n.appendChild(b);\n"
    "})();\n"
    "</script>\n"
    "</body>\n"
    "</html>\n";

/* Create theme files */
static int create_theme_files(void)
{
    write_file("themes/default/style.css", css_template);
    printf("Created: themes/default/style.css\n");
    
    write_file("themes/default/post.html", html_template);
    printf("Created: themes/default/post.html\n");
    
    write_file("themes/default/index.html", index_template);
    printf("Created: themes/default/index.html\n");
    
    write_file("themes/default/tag.html", tag_template);
    printf("Created: themes/default/tag.html\n");
    
    write_file("themes/default/archive.html", archive_template);
    printf("Created: themes/default/archive.html\n");
    return CXO_OK;
}

/* Initialize new project */
int cmd_init(const char* dir)
{
    int rc;
    
    /* Create project directory */
    if (dir && strlen(dir) > 0 && strcmp(dir, ".") != 0) {
        rc = mkdir_p(dir);
        if (CXO_IS_ERR(rc)) {
            fprintf(stderr, "Error: Cannot create directory %s\n", dir);
            return CXO_ERR_IO;
        }
        if (chdir(dir) != 0) {
            fprintf(stderr, "Error: Cannot enter directory %s\n", dir);
            return CXO_ERR_IO;
        }
        printf("Creating project in: %s\n", dir);
    } else {
        printf("Creating project in current directory\n");
    }
    
    create_project_dirs();
    
    /* Create config.toml */
    rc = write_file("config.toml", config_template);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Error: Failed to create config.toml\n");
        return rc;
    }
    printf("Created: config.toml\n");
    
    create_theme_files();
    create_sample_post();
    
    printf("\nProject initialized successfully!\n");
    printf("Run 'cxo build' to generate your site.\n");
    
    return CXO_OK;
}

/* Generate slug from title */
static void generate_slug(const char* title, char* slug, size_t size)
{
    size_t i;
    size_t j;
    
    j = 0;
    for (i = 0; title[i] && j < size - 1; i++) {
        char c = title[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            slug[j++] = c;
        } else if (c >= 'A' && c <= 'Z') {
            slug[j++] = c + 32;
        } else if ((c == ' ' || c == '-' || c == '_') && j > 0 && slug[j-1] != '-') {
            slug[j++] = '-';
        }
    }
    
    if (j > 0 && slug[j-1] == '-') {
        j--;
    }
    slug[j] = '\0';
}

/* Create new post */
int cmd_new(const char* title)
{
    char slug[128];
    char date[16];
    char filename[256];
    char content[512];
    struct stat st;
    int rc;
    
    /* Check if in CXO project */
    if (stat("config.toml", &st) != 0) {
        fprintf(stderr, "Error: Not a CXO project (config.toml not found)\n");
        fprintf(stderr, "Run 'cxo init' first.\n");
        return CXO_ERR_IO;
    }
    
    /* Ensure default language content dir exists */
    {
        char dir[64];
        snprintf(dir, sizeof(dir), "content/%s", CXO_LANGS[0].code);
        mkdir_p(dir);
    }

    /* Generate slug */
    generate_slug(title, slug, sizeof(slug));
    if (strlen(slug) == 0) {
        fprintf(stderr, "Error: Invalid title\n");
        return CXO_ERR_INVAL;
    }

    /* Check if exists */
    get_current_date(date, sizeof(date));
    snprintf(filename, sizeof(filename), "content/%s/%s.md",
             CXO_LANGS[0].code, slug);
    if (stat(filename, &st) == 0) {
        fprintf(stderr, "Error: File already exists: %s\n", filename);
        return CXO_ERR_IO;
    }
    
    /* Create post */
    snprintf(content, sizeof(content),
             "---\n"
             "id: %s\n"
             "title: %s\n"
             "date: %s\n"
             "---\n"
             "\n"
             "Write your content here...\n",
             slug, title, date);
    
    rc = write_file(filename, content);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Error: Failed to create %s\n", filename);
        return rc;
    }
    
    printf("Created: %s\n", filename);
    return CXO_OK;
}

/* Clean build output */
int cmd_clean(void)
{
    printf("Cleaning public/ directory...\n");
#ifdef _WIN32
    system("rmdir /s /q public 2>nul && mkdir public");
#else
    system("rm -rf public/*");
#endif
    printf("Clean complete.\n");
    return CXO_OK;
}
