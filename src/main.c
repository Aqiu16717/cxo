/*
 * main.c - CXO Static Blog Engine Main Entry
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../include/cxo.h"

/* External command functions */
extern int cmd_build(void);
extern int cmd_init(const char* dir);
extern int cmd_new(const char* title);
extern int cmd_clean(void);
extern int cmd_serve(int port, int rebuild);
extern int cmd_deploy(void);

static void print_usage(const char* prog)
{
    fprintf(stderr, "Usage: %s <command> [options]\n", prog);
    fprintf(stderr, "\nCommands:\n");
    fprintf(stderr, "  init [dir]     Initialize a new CXO project\n");
    fprintf(stderr, "  new <title>    Create a new blog post\n");
    fprintf(stderr, "  build          Build the static site\n");
    fprintf(stderr, "  serve [options] [port]  Start development server (default: 8080)\n");
    fprintf(stderr, "                  -w, --watch   Enable hot reload\n");
    fprintf(stderr, "  deploy         Deploy to GitHub Pages\n");
    fprintf(stderr, "  clean          Clean build output\n");
    fprintf(stderr, "  version        Show version information\n");
    fprintf(stderr, "  help           Show this help message\n");
}

static void print_version(void)
{
    printf("CXO %s - Minimalist Static Blog Engine\n", CXO_VERSION);
    printf("Copyright (c) 2026 Aq!u\n");
    printf("MIT License\n");
}

int main(int argc, char* argv[])
{
    const char* cmd;
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    cmd = argv[1];
    
    if (strcmp(cmd, "init") == 0) {
        /* cxo init [dir] */
        const char* dir = (argc >= 3) ? argv[2] : ".";
        int rc = cmd_init(dir);
        return CXO_IS_ERR(rc) ? 1 : 0;
    } else if (strcmp(cmd, "new") == 0) {
        /* cxo new <title> */
        if (argc < 3) {
            fprintf(stderr, "Error: Missing title\n");
            return 1;
        }
        int rc = cmd_new(argv[2]);
        return CXO_IS_ERR(rc) ? 1 : 0;
    } else if (strcmp(cmd, "serve") == 0) {
        int port = 8080;
        int watch = 0;
        int arg_idx = 2;
        
        /* Parse options */
        while (arg_idx < argc) {
            if (strcmp(argv[arg_idx], "-w") == 0 ||
                strcmp(argv[arg_idx], "--watch") == 0) {
                watch = 1;
                arg_idx++;
            } else if (argv[arg_idx][0] == '-') {
                fprintf(stderr, "Error: Unknown option: %s\n", argv[arg_idx]);
                return 1;
            } else {
                /* Port number */
                break;
            }
        }
        
        /* Parse port */
        if (arg_idx < argc) {
            port = atoi(argv[arg_idx]);
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "Error: Invalid port number\n");
                return 1;
            }
        }
        
        int rc = cmd_serve(port, watch);
        return CXO_IS_ERR(rc) ? 1 : 0;
    } else if (strcmp(cmd, "deploy") == 0) {
        int rc = cmd_deploy();
        return CXO_IS_ERR(rc) ? 1 : 0;
    } else if (strcmp(cmd, "clean") == 0) {
        int rc = cmd_clean();
        return CXO_IS_ERR(rc) ? 1 : 0;
    } else if (strcmp(cmd, "build") == 0 || strcmp(cmd, "g") == 0) {
        int rc = cmd_build();
        return CXO_IS_ERR(rc) ? 1 : 0;
    } else if (strcmp(cmd, "version") == 0 || strcmp(cmd, "-v") == 0) {
        print_version();
        return 0;
    } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
}
