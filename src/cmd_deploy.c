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

/* Check if command exists */
static int cmd_exists(const char* cmd)
{
    char check_cmd[256];
    snprintf(check_cmd, sizeof(check_cmd), "which %s > /dev/null 2>&1", cmd);
    return system(check_cmd) == 0;
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
    printf("Building site...\n");
    /* Clean and rebuild to ensure no stale files */
    system("rm -rf public/");
    if (system("./cxo build") != 0) {
        fprintf(stderr, "Error: Build failed\n");
        return -1;
    }
    return 0;
}

/* Deploy to gh-pages branch */
static int deploy_to_gh_pages(void)
{
    char current_branch[256];
    char commit_hash[64];
    int ret;
    
    /* Get current branch */
    exec_cmd("git branch --show-current", current_branch, sizeof(current_branch));
    if (strlen(current_branch) == 0) {
        strcpy(current_branch, "main");
    }
    
    /* Get current commit hash */
    exec_cmd("git rev-parse --short HEAD", commit_hash, sizeof(commit_hash));
    if (strlen(commit_hash) == 0) {
        strcpy(commit_hash, "unknown");
    }
    
    printf("Deploying to gh-pages branch...\n");
    
    /* Check if gh-pages branch exists */
    ret = system("git show-ref --verify --quiet refs/heads/gh-pages");
    if (ret != 0) {
        /* Create orphan branch */
        ret = system("git checkout --orphan gh-pages");
        if (ret != 0) {
            fprintf(stderr, "Error: Failed to create gh-pages branch\n");
            return -1;
        }
    } else {
        ret = system("git checkout gh-pages");
        if (ret != 0) {
            fprintf(stderr, "Error: Failed to checkout gh-pages branch\n");
            return -1;
        }
    }
    
    /* Remove all files except public/ */
    system("git rm -rf . > /dev/null 2>&1");
    
    /* Move public/* to root */
    system("mv public/* . 2>/dev/null; mv public/.* . 2>/dev/null; rmdir public 2>/dev/null");
    
    /* Add all files */
    ret = system("git add -A");
    if (ret != 0) {
        fprintf(stderr, "Error: Failed to add files\n");
        goto cleanup;
    }
    
    /* Commit */
    char commit_msg[512];
    snprintf(commit_msg, sizeof(commit_msg),
             "git commit -m \"Deploy: %s from %s\"",
             commit_hash, current_branch);
    ret = system(commit_msg);
    if (ret != 0) {
        /* No changes to commit */
        printf("No changes to deploy\n");
    } else {
        printf("Committed changes\n");
    }
    
    /* Push */
    ret = system("git push origin gh-pages");
    if (ret != 0) {
        fprintf(stderr, "Error: Failed to push to gh-pages\n");
        goto cleanup;
    }
    
    printf("Pushed to gh-pages branch\n");
    
    /* Switch back to original branch */
    char checkout_cmd[256];
    snprintf(checkout_cmd, sizeof(checkout_cmd), "git checkout %s", current_branch);
    system(checkout_cmd);
    
    printf("\n✓ Deployed successfully!\n");
    printf("Your site will be available at:\n");
    printf("  https://<username>.github.io/<repository>/\n");
    printf("\nNote: It may take a few minutes for GitHub Pages to update.\n");
    
    return 0;
    
cleanup:
    /* Try to switch back */
    {
        char checkout_cmd[256];
        snprintf(checkout_cmd, sizeof(checkout_cmd), "git checkout %s", current_branch);
        system(checkout_cmd);
    }
    return -1;
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
