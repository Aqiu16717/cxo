/*
 * main.c - CXO Static Blog Engine Main Entry
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../include/cxo.h"

/* External command functions */
extern int cmd_build(void);
extern int cmd_init(const char* dir);
extern int cmd_new(const char* title);
extern int cmd_clean(void);
extern int cmd_serve(int port, int rebuild);
extern int cmd_deploy(void);

/* Command table entry */
typedef struct {
    const char* name;
    /* short form, NULL if none */
    const char* alias;
    /* argument synopsis for help, NULL if none */
    const char* args;
    const char* desc;
    int (*run)(int argc, char** argv);
} cxo_cmd_t;

static void print_version(void)
{
    printf("CXO %s - Minimalist Static Blog Engine\n", CXO_VERSION);
    printf("Copyright (c) 2026 Aq!u\n");
    printf("MIT License\n");
}

static int run_init(int argc, char** argv)
{
    return cmd_init(argc >= 1 ? argv[0] : ".");
}

static int run_new(int argc, char** argv)
{
    if (argc < 1) {
        fprintf(stderr, "Error: Missing title\n");
        return 1;
    }
    return cmd_new(argv[0]);
}

static int run_build(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    return cmd_build();
}

static int run_serve(int argc, char** argv)
{
    int port = 8080;
    int watch = 0;
    int i = 0;

    /* Parse options */
    while (i < argc) {
        if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--watch") == 0) {
            watch = 1;
            i++;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            return 1;
        } else {
            break;
        }
    }

    /* Parse port */
    if (i < argc) {
        port = atoi(argv[i]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Error: Invalid port number\n");
            return 1;
        }
    }

    return cmd_serve(port, watch);
}

static int run_clean(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    return cmd_clean();
}

static int run_deploy(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    return cmd_deploy();
}

static int run_version(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    print_version();
    return 0;
}

static void print_commands(void);
static void print_usage(const char* prog);

static const char* g_prog = "cxo";

static int run_help(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    print_usage(g_prog);
    return 0;
}

/* Command table - add a new command by registering one row */
static const cxo_cmd_t COMMANDS[] = {
    { "init",    NULL, "[dir]",   "Initialize a new CXO project", run_init },
    { "new",     NULL, "<title>", "Create a new blog post", run_new },
    { "build",   "g",  NULL,      "Build the static site", run_build },
    { "serve",   "s",  "[-w|--watch] [port]",
      "Start dev server (default port 8080)", run_serve },
    { "deploy",  NULL, NULL,      "Deploy to GitHub Pages", run_deploy },
    { "clean",   NULL, NULL,      "Clean build output", run_clean },
    { "version", "v",  NULL,      "Show version information", run_version },
    { "help",    "h",  NULL,      "Show this help message", run_help },
};

#define COMMAND_COUNT (sizeof(COMMANDS) / sizeof(COMMANDS[0]))

static void print_commands(void)
{
    size_t i;

    fprintf(stderr, "\nCommands:\n");
    for (i = 0; i < COMMAND_COUNT; i++) {
        char display[48];

        snprintf(display, sizeof(display), "%s%s%s",
                 COMMANDS[i].name,
                 COMMANDS[i].args ? " " : "",
                 COMMANDS[i].args ? COMMANDS[i].args : "");
        fprintf(stderr, "  %-28s %s\n", display, COMMANDS[i].desc);
    }
}

static void print_usage(const char* prog)
{
    fprintf(stderr, "Usage: %s <command> [options]\n", prog);
    print_commands();
}

/* Match arg against name, alias, and their dashed forms (-v, --version) */
static int cmd_matches(const cxo_cmd_t* cmd, const char* arg)
{
    char dashed[32];

    if (strcmp(arg, cmd->name) == 0) {
        return 1;
    }
    snprintf(dashed, sizeof(dashed), "--%s", cmd->name);
    if (strcmp(arg, dashed) == 0) {
        return 1;
    }
    if (cmd->alias) {
        if (strcmp(arg, cmd->alias) == 0) {
            return 1;
        }
        snprintf(dashed, sizeof(dashed), "-%s", cmd->alias);
        if (strcmp(arg, dashed) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    const char* cmd;
    size_t i;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    cmd = argv[1];
    g_prog = argv[0];
    for (i = 0; i < COMMAND_COUNT; i++) {
        if (cmd_matches(&COMMANDS[i], cmd)) {
            int rc = COMMANDS[i].run(argc - 2, argv + 2);
            return CXO_IS_ERR(rc) ? 1 : rc;
        }
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    print_usage(argv[0]);
    return 1;
}
