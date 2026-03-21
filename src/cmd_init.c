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
#include "../include/cxo.h"

/* Default config.toml template */
static const char* config_template =
    "[site]\n"
    "title = \"My Blog\"\n"
    "description = \"A minimalist blog powered by CXO\"\n"
    "base_url = \"https://example.com\"\n"
    "\n"
    "[theme]\n"
    "path = \"themes/default\"\n";

/* Default CSS */
static const char* css_template =
    "body {\n"
    "  font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Helvetica, Arial, sans-serif;\n"
    "  max-width: 800px;\n"
    "  margin: 40px auto;\n"
    "  padding: 0 20px;\n"
    "  line-height: 1.6;\n"
    "  color: #333;\n"
    "}\n";

/* Default HTML template */
static const char* html_template =
    "<!DOCTYPE html>\n"
    "<html lang=\"{{lang}}\">\n"
    "<head>\n"
    "<meta charset=\"UTF-8\">\n"
    "<title>{{title}}</title>\n"
    "<link rel=\"stylesheet\" href=\"/style.css\">\n"
    "</head>\n"
    "<body>\n"
    "<nav><a href=\"/\">{{site_title}}</a></nav>\n"
    "<article>\n"
    "<h1>{{title}}</h1>\n"
    "<div class=\"content\">{{content}}</div>\n"
    "</article>\n"
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
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
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

/* Initialize new project */
int cmd_init(const char* dir)
{
    int rc;
    char date[16];
    char post_content[512];
    
    /* Create project directory */
    if (dir && strlen(dir) > 0 && strcmp(dir, ".") != 0) {
        rc = mkdir_p(dir);
        if (CXO_IS_ERR(rc)) {
            fprintf(stderr, "Error: Cannot create directory %s\n", dir);
            return 1;
        }
        if (chdir(dir) != 0) {
            fprintf(stderr, "Error: Cannot enter directory %s\n", dir);
            return 1;
        }
        printf("Creating project in: %s\n", dir);
    } else {
        printf("Creating project in current directory\n");
    }
    
    /* Create directories */
    mkdir_p("content/zh");
    mkdir_p("content/en");
    mkdir_p("themes/default");
    
    /* Create config.toml */
    rc = write_file("config.toml", config_template);
    if (CXO_IS_ERR(rc)) {
        fprintf(stderr, "Error: Failed to create config.toml\n");
        return 1;
    }
    printf("Created: config.toml\n");
    
    /* Create theme files */
    write_file("themes/default/style.css", css_template);
    printf("Created: themes/default/style.css\n");
    
    write_file("themes/default/post.html", html_template);
    printf("Created: themes/default/post.html\n");
    
    /* Create sample post */
    get_current_date(date, sizeof(date));
    snprintf(post_content, sizeof(post_content),
             "---\n"
             "id: hello\n"
             "title: Hello World\n"
             "date: %s\n"
             "---\n"
             "\n"
             "Welcome to CXO!\n", date);
    
    write_file("content/zh/hello.md", post_content);
    printf("Created: content/zh/hello.md\n");
    
    printf("\nProject initialized successfully!\n");
    printf("Run 'cxo build' to generate your site.\n");
    
    return 0;
}
