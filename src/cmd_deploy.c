/*
 * cmd_deploy.c - Deploy to GitHub Pages
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include "../include/cxo.h"

#define BUFFER_SIZE 1024

/* Execute shell command and capture output */
static int exec_cmd(const char* cmd, char* output, size_t size)
{
    FILE* fp;
    size_t len;
    
    fp = popen(cmd, "r");
    if (!fp) {
        return -1;
    }
    
    if (output && size > 0) {
        if (fgets(output, size, fp)) {
            len = strlen(output);
            if (len > 0 && output[len - 1] == '\n') {
                output[len - 1] = '\0';
            }
        } else {
            output[0] = '\0';
        }
    }
    
    return pclose(fp);
}

/* Run command via execvp to avoid shell injection */
static int run_cmd(const char* argv[])
{
    pid_t pid;
    int status;
    
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    
    if (pid == 0) {
        execvp(argv[0], (char* const*)argv);
        _exit(127);
    }
    
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

/* Check if command exists */
static int cmd_exists(const char* cmd)
{
    const char* argv[] = {"which", cmd, NULL};
    int devnull = open("/dev/null", O_WRONLY);
    int saved_stderr = dup(STDERR_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);
    int ret;
    
    if (devnull < 0) {
        return 0;
    }
    
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);
    close(devnull);
    
    ret = run_cmd(argv);
    
    dup2(saved_stdout, STDOUT_FILENO);
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stdout);
    close(saved_stderr);
    
    return ret == 0;
}

/* Get git remote URL */
static int get_remote_url(char* url, size_t size)
{
    return exec_cmd("git remote get-url origin 2>/dev/null", url, size);
}

/* Check if we're in a git repo */
static int is_git_repo(void)
{
    return system("git rev-parse --git-dir > /dev/null 2>&1") == 0;
}

/* Build site before deploy */
static int build_site(void)
{
    const char* argv[] = {"./cxo", "build", NULL};
    printf("Building site...\n");
    /* Clean and rebuild to ensure no stale files */
    system("rm -rf public/");
    if (run_cmd(argv) != 0) {
        fprintf(stderr, "Error: Build failed\n");
        return -1;
    }
    return 0;
}

/* Get current git branch and short hash */
static void get_git_info(char* branch, size_t branch_size,
                         char* hash, size_t hash_size)
{
    exec_cmd("git branch --show-current", branch, branch_size);
    if (strlen(branch) == 0) {
        strcpy(branch, "main");
    }
    
    exec_cmd("git rev-parse --short HEAD", hash, hash_size);
    if (strlen(hash) == 0) {
        strcpy(hash, "unknown");
    }
}

/* Switch to gh-pages branch, creating it if needed */
static int checkout_gh_pages(void)
{
    const char* check_argv[] = {"git", "show-ref", "--verify", "--quiet",
                                "refs/heads/gh-pages", NULL};
    int ret = run_cmd(check_argv);
    
    if (ret != 0) {
        const char* orphan_argv[] = {"git", "checkout", "--orphan", "gh-pages", NULL};
        ret = run_cmd(orphan_argv);
        if (ret != 0) {
            fprintf(stderr, "Error: Failed to create gh-pages branch\n");
            return -1;
        }
    } else {
        const char* checkout_argv[] = {"git", "checkout", "gh-pages", NULL};
        ret = run_cmd(checkout_argv);
        if (ret != 0) {
            fprintf(stderr, "Error: Failed to checkout gh-pages branch\n");
            return -1;
        }
    }
    
    return 0;
}

/* Add, commit and push to gh-pages */
static int commit_and_push(const char* branch, const char* hash)
{
    char msg[256];
    const char* add_argv[] = {"git", "add", "-A", NULL};
    const char* push_argv[] = {"git", "push", "origin", "gh-pages", NULL};
    const char* commit_argv[] = {"git", "commit", "-m", msg, NULL};
    int ret;
    
    ret = run_cmd(add_argv);
    if (ret != 0) {
        fprintf(stderr, "Error: Failed to add files\n");
        return -1;
    }
    
    snprintf(msg, sizeof(msg), "Deploy: %s from %s", hash, branch);
    ret = run_cmd(commit_argv);
    if (ret != 0) {
        printf("No changes to deploy\n");
    } else {
        printf("Committed changes\n");
    }
    
    ret = run_cmd(push_argv);
    if (ret != 0) {
        fprintf(stderr, "Error: Failed to push to gh-pages\n");
        return -1;
    }
    
    printf("Pushed to gh-pages branch\n");
    return 0;
}

/* Switch back to original branch */
static void switch_back(const char* branch)
{
    const char* checkout_argv[] = {"git", "checkout", branch, NULL};
    run_cmd(checkout_argv);
}

/* Deploy to gh-pages branch */
static int deploy_to_gh_pages(void)
{
    char current_branch[256];
    char commit_hash[64];
    
    get_git_info(current_branch, sizeof(current_branch),
                 commit_hash, sizeof(commit_hash));
    
    printf("Deploying to gh-pages branch...\n");
    
    if (checkout_gh_pages() != 0) {
        return -1;
    }
    
    /* Remove all files except public/ */
    system("git rm -rf . > /dev/null 2>&1");
    
    /* Move public files to root */
    system("mv public/* . 2>/dev/null; mv public/.* . 2>/dev/null; rmdir public 2>/dev/null");
    
    if (commit_and_push(current_branch, commit_hash) != 0) {
        switch_back(current_branch);
        return -1;
    }
    
    switch_back(current_branch);
    
    printf("\n✓ Deployed successfully!\n");
    printf("Your site will be available at:\n");
    printf("  https://<username>.github.io/<repository>/\n");
    printf("\nNote: It may take a few minutes for GitHub Pages to update.\n");
    
    return 0;
}

/* Deploy command */
int cmd_deploy(void)
{
    char remote_url[512];
    
    /* Check prerequisites */
    if (!cmd_exists("git")) {
        fprintf(stderr, "Error: git is not installed\n");
        return CXO_ERR_IO;
    }
    
    if (!is_git_repo()) {
        fprintf(stderr, "Error: Not a git repository\n");
        fprintf(stderr, "Run 'git init' first\n");
        return CXO_ERR_IO;
    }
    
    /* Check remote */
    if (get_remote_url(remote_url, sizeof(remote_url)) != 0 || strlen(remote_url) == 0) {
        fprintf(stderr, "Error: No git remote configured\n");
        fprintf(stderr, "Run 'git remote add origin <url>' first\n");
        return CXO_ERR_IO;
    }
    
    printf("Remote: %s\n", remote_url);
    
    /* Check if it's GitHub */
    if (strstr(remote_url, "github.com") == NULL) {
        fprintf(stderr, "Warning: Remote doesn't appear to be GitHub\n");
        fprintf(stderr, "GitHub Pages deployment may not work\n");
    }
    
    /* Build site */
    if (build_site() != 0) {
        return CXO_ERR_IO;
    }
    
    /* Check if public/ exists */
    struct stat st;
    if (stat("public", &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: public/ directory not found after build\n");
        return CXO_ERR_IO;
    }
    
    /* Deploy */
    if (deploy_to_gh_pages() != 0) {
        return CXO_ERR_IO;
    }
    
    return CXO_OK;
}
