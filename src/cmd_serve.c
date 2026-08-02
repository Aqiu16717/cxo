/*
 * cmd_serve.c - Development HTTP server for CXO
 * Copyright (c) 2026 Aq!u
 * MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include "../include/platform.h"
#include "../include/cxo.h"

#ifndef _WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#endif

#define DEFAULT_PORT 8080
#define DEFAULT_ROOT "public"
#define BUFFER_SIZE 8192
#define CXO_MAX_PATH 512
#define MAX_WATCH_PATHS 256

/* External command functions */
extern int cmd_build(void);

/* Platform-specific defines */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* MIME type mapping */
typedef struct {
    const char* ext;
    const char* mime;
} mime_map_t;

static const mime_map_t mime_types[] = {
    {".html", "text/html; charset=utf-8"},
    {".htm", "text/html; charset=utf-8"},
    {".css", "text/css; charset=utf-8"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".txt", "text/plain"},
    {".xml", "application/xml"},
    {".md", "text/markdown"},
    {NULL, NULL}
};

/* Server state */
static volatile int server_running = 1;

/* Watch state */
typedef struct {
    char path[CXO_MAX_PATH];
    time_t mtime;
} watch_path_t;

static watch_path_t watch_paths[MAX_WATCH_PATHS];
static int watch_count = 0;

#ifndef _WIN32
/* Signal handler for graceful shutdown */
static void signal_handler(int sig)
{
    (void)sig;
    server_running = 0;
}
#endif

/* Get MIME type from file extension */
static const char* get_mime_type(const char* path)
{
    const char* dot;
    size_t i;
    
    dot = strrchr(path, '.');
    if (!dot) {
        return "application/octet-stream";
    }
    
    for (i = 0; mime_types[i].ext; i++) {
        if (strcasecmp(dot, mime_types[i].ext) == 0) {
            return mime_types[i].mime;
        }
    }
    
    return "application/octet-stream";
}

/* URL decode */
static void url_decode(char* dst, const char* src, size_t size)
{
    size_t i;
    size_t j;
    
    j = 0;
    for (i = 0; src[i] && j < size - 1; i++) {
        if (src[i] == '%' && src[i + 1] && src[i + 2]) {
            unsigned int c;
            if (sscanf(src + i + 1, "%2x", &c) == 1) {
                dst[j++] = (char)c;
                i += 2;
            } else {
                dst[j++] = src[i];
            }
        } else if (src[i] == '+') {
            dst[j++] = ' ';
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

/* Check if path contains directory traversal */
static int has_traversal(const char* path)
{
    const char* p;
    
    p = path;
    while (*p) {
        if (p[0] == '.' && p[1] == '.') {
            if ((p == path || p[-1] == '/') &&
                (p[2] == '\0' || p[2] == '/')) {
                return 1;
            }
        }
        p++;
    }
    return 0;
}

/* Send HTTP response */
static void send_response(int client, int status, const char* status_text,
                          const char* content_type, const char* body,
                          size_t body_len)
{
    char header[512];
    time_t now;
    struct tm* tm_info;
    char date[64];
    cxo_ssize_t sent;
    size_t total;
    
    time(&now);
    tm_info = gmtime(&now);
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", tm_info);
    
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %lu\r\n"
             "Date: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             status, status_text, content_type, (unsigned long)body_len, date);
    
    total = 0;
    while (total < strlen(header)) {
        sent = send(client, header + total, strlen(header) - total, 0);
        if (sent <= 0) {
            return;
        }
        total += sent;
    }
    
    if (body && body_len > 0) {
        total = 0;
        while (total < body_len) {
            sent = send(client, body + total, body_len - total, 0);
            if (sent <= 0) {
                return;
            }
            total += sent;
        }
    }
}

/* Send SSE headers for hot reload */
static void send_sse_headers(int client)
{
    char header[256];
    cxo_ssize_t sent;
    size_t total;
    
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/event-stream\r\n"
             "Cache-Control: no-cache\r\n"
             "Connection: keep-alive\r\n"
             "\r\n");
    
    total = 0;
    while (total < strlen(header)) {
        sent = send(client, header + total, strlen(header) - total, 0);
        if (sent <= 0) {
            return;
        }
        total += sent;
    }
}

/* Send reload event via SSE */
static void send_reload_event(int client)
{
    const char* event = "data: reload\n\n";
    send(client, event, strlen(event), MSG_NOSIGNAL);
}

static void send_file_response_full(int client, const char* path, int is_head)
{
    int fd;
    struct stat st;
    char header[512];
    char buf[BUFFER_SIZE];
    cxo_ssize_t n;
    cxo_ssize_t sent;
    size_t total;
    time_t now;
    struct tm* tm_info;
    char date[64];
    const char* mime;
    
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        send_response(client, 404, "Not Found", "text/html",
                      "<h1>404 Not Found</h1>", 22);
        return;
    }
    
    if (fstat(fd, &st) < 0) {
        close(fd);
        send_response(client, 500, "Internal Server Error", "text/html",
                      "<h1>500 Internal Server Error</h1>", 33);
        return;
    }
    
    if (S_ISDIR(st.st_mode)) {
        close(fd);
        send_response(client, 403, "Forbidden", "text/html",
                      "<h1>403 Forbidden</h1>", 22);
        return;
    }
    
    mime = get_mime_type(path);
    
    time(&now);
    tm_info = gmtime(&now);
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", tm_info);
    
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Date: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             mime, (long)st.st_size, date);
    
    total = 0;
    while (total < strlen(header)) {
        sent = send(client, header + total, strlen(header) - total, 0);
        if (sent <= 0) {
            close(fd);
            return;
        }
        total += sent;
    }
    
    if (is_head) {
        close(fd);
        return;
    }
    
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        total = 0;
        while (total < (size_t)n) {
            sent = send(client, buf + total, n - total, 0);
            if (sent <= 0) {
                close(fd);
                return;
            }
            total += sent;
        }
    }
    
    close(fd);
}

#define send_file_response(client, path) \
    do { send_file_response_full((client), (path), 0); } while (0)

/* Forward declaration */
static void send_directory_listing(int client, const char* root, const char* uri);

/* Check if SSE client is still connected by sending a ping */
static int check_sse_client(int client)
{
    char ping[] = ":ping\n\n";
    cxo_ssize_t sent;

    if (client < 0) {
        return 0;
    }

    sent = send(client, ping, strlen(ping), MSG_NOSIGNAL);
    if (sent < 0) {
        return 0;
    }
    return 1;
}

/* Set of connected SSE clients (multiple browser tabs) */
#define MAX_SSE_CLIENTS 8

typedef struct {
    int fds[MAX_SSE_CLIENTS];
    size_t count;
} sse_set_t;

/* Check whether a fd is a registered SSE client */
static int sse_contains(const sse_set_t* set, int fd)
{
    size_t i;

    for (i = 0; i < set->count; i++) {
        if (set->fds[i] == fd) {
            return 1;
        }
    }
    return 0;
}

/* Remove the fd at index, closing the socket */
static void sse_remove_at(sse_set_t* set, size_t index)
{
    cxo_close_socket(set->fds[index]);
    if (index + 1 < set->count) {
        memmove(set->fds + index, set->fds + index + 1,
                (set->count - index - 1) * sizeof(set->fds[0]));
    }
    set->count--;
}

/* Register a new SSE client; evicts the oldest when full */
static void sse_add(sse_set_t* set, int fd)
{
    if (sse_contains(set, fd)) {
        return;
    }
    if (set->count >= MAX_SSE_CLIENTS) {
        sse_remove_at(set, 0);
    }
    set->fds[set->count++] = fd;
}

/* Parse HTTP request line, returns 0 on success */
static int parse_request_line(const char* buf, char* method, char* uri, int* is_head)
{
    char* query;
    
    if (sscanf(buf, "%15s %511s", method, uri) != 2) {
        return -1;
    }
    
    *is_head = (strcmp(method, "HEAD") == 0);
    if (strcmp(method, "GET") != 0 && !*is_head) {
        return -1;
    }
    
    query = strchr(uri, '?');
    if (query) {
        *query = '\0';
    }
    
    return 0;
}

/* Serve a file, directory, or .html fallback */
static void serve_request_path(int client, const char* root,
                               const char* decoded_uri, int is_head)
{
    char path[CXO_MAX_PATH];
    struct stat st;
    
    {
        int n;
        if (decoded_uri[0] == '/') {
            n = snprintf(path, sizeof(path), "%s%s", root, decoded_uri);
        } else {
            n = snprintf(path, sizeof(path), "%s/%s", root, decoded_uri);
        }
        if (n < 0 || (size_t)n >= sizeof(path)) {
            send_response(client, 414, "URI Too Long", "text/html",
                          "<h1>414 URI Too Long</h1>", 24);
            return;
        }
    }
    
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            char index_path[CXO_MAX_PATH];
            int ret;
            
            ret = snprintf(index_path, sizeof(index_path),
                           "%s/index.html", path);
            if (ret < 0 || (size_t)ret >= sizeof(index_path)) {
                send_directory_listing(client, root, decoded_uri);
                return;
            }
            
            if (stat(index_path, &st) == 0 && !S_ISDIR(st.st_mode)) {
                send_file_response_full(client, index_path, is_head);
            } else if (is_head) {
                send_response(client, 200, "OK", "text/html; charset=utf-8", NULL, 0);
            } else {
                send_directory_listing(client, root, decoded_uri);
            }
        } else {
            send_file_response_full(client, path, is_head);
        }
        return;
    }
    
    /* Try adding .html extension */
    {
        char html_path[CXO_MAX_PATH + 5];
        int ret;
        
        ret = snprintf(html_path, sizeof(html_path), "%s.html", path);
        if (ret < 0 || (size_t)ret >= sizeof(html_path)) {
            send_response(client, 404, "Not Found", "text/html",
                          "<h1>404 Not Found</h1>", 22);
            return;
        }
        
        if (stat(html_path, &st) == 0 && !S_ISDIR(st.st_mode)) {
            send_file_response_full(client, html_path, is_head);
        } else {
            send_response(client, 404, "Not Found", "text/html",
                          "<h1>404 Not Found</h1>", 22);
        }
    }
}

/* Handle single HTTP request */
static void handle_request(int client, const char* root, sse_set_t* sse_clients)
{
    char buf[BUFFER_SIZE];
    char method[16];
    char uri[CXO_MAX_PATH];
    char decoded_uri[CXO_MAX_PATH];
    cxo_ssize_t n;
    int is_head;

    n = recv(client, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        return;
    }
    buf[n] = '\0';

    if (parse_request_line(buf, method, uri, &is_head) != 0) {
        send_response(client, 400, "Bad Request", "text/html",
                      "<h1>400 Bad Request</h1>", 24);
        return;
    }

    url_decode(decoded_uri, uri, sizeof(decoded_uri));

    /* Check for SSE endpoint */
    if (strcmp(decoded_uri, "/__cxo_reload") == 0) {
        send_sse_headers(client);
        sse_add(sse_clients, client);
        return;
    }
    
    /* Prevent directory traversal in URI */
    if (has_traversal(decoded_uri)) {
        send_response(client, 403, "Forbidden", "text/html",
                      "<h1>403 Forbidden</h1>", 22);
        return;
    }
    
    serve_request_path(client, root, decoded_uri, is_head);
}

/* Generate directory listing HTML */
static void send_directory_listing(int client, const char* root, const char* uri)
{
    char path[CXO_MAX_PATH];
    DIR* dir;
    struct dirent* entry;
    char html[4096];
    size_t html_len;
    int is_root;
    
    {
        int n = snprintf(path, sizeof(path), "%s%s", root, uri);
        if (n < 0 || (size_t)n >= sizeof(path)) {
            send_response(client, 414, "URI Too Long", "text/html",
                          "<h1>414 URI Too Long</h1>", 24);
            return;
        }
    }
    dir = opendir(path);
    if (!dir) {
        send_response(client, 403, "Forbidden", "text/html",
                      "<h1>403 Forbidden</h1>", 22);
        return;
    }
    
    is_root = (strcmp(uri, "/") == 0);
    
    {
        int n = snprintf(html, sizeof(html),
                         "<!DOCTYPE html>\n"
                         "<html>\n"
                         "<head>\n"
                         "<meta charset=\"UTF-8\">\n"
                         "<title>Index of %s</title>\n"
                         "<style>\n"
                         "body { font-family: -apple-system, sans-serif; max-width: 800px; margin: 40px auto; padding: 0 20px; }\n"
                         "h1 { border-bottom: 1px solid #ddd; padding-bottom: 10px; }\n"
                         "ul { list-style: none; padding: 0; }\n"
                         "li { padding: 8px 0; border-bottom: 1px solid #f0f0f0; }\n"
                         "a { text-decoration: none; color: #0366d6; }\n"
                         "a:hover { text-decoration: underline; }\n"
                         ".dir { font-weight: bold; }\n"
                         "</style>\n"
                         "</head>\n"
                         "<body>\n"
                         "<h1>Index of %s</h1>\n"
                         "<ul>\n",
                         uri, uri);
        html_len = (n > 0) ? (size_t)n : 0;
    }
    
    if (!is_root) {
        html_len += snprintf(html + html_len, sizeof(html) - html_len,
                             "<li><a href=\"..\">../</a></li>\n");
    }
    
    while ((entry = readdir(dir)) != NULL) {
        const char* name = entry->d_name;
        struct stat entry_st;
        char full_path[CXO_MAX_PATH];
        int is_dir;
        int n;
        
        if (name[0] == '.' || strcmp(name, "..") == 0) {
            continue;
        }
        
        {
            int n = snprintf(full_path, sizeof(full_path), "%s/%s", path, name);
            if (n < 0 || (size_t)n >= sizeof(full_path)) {
                continue;
            }
        }
        is_dir = (stat(full_path, &entry_st) == 0 && S_ISDIR(entry_st.st_mode));
        
        n = snprintf(html + html_len, sizeof(html) - html_len,
                     "<li><a href=\"%s%s\" %s>%s%s</a></li>\n",
                     name, is_dir ? "/" : "",
                     is_dir ? "class=\"dir\"" : "",
                     name, is_dir ? "/" : "");
        if (n > 0) {
            html_len += (size_t)n;
        }
        
        if (html_len >= sizeof(html) - 256) {
            break;
        }
    }
    
    closedir(dir);
    
    {
        int n = snprintf(html + html_len, sizeof(html) - html_len,
                         "</ul>\n"
                         "</body>\n"
                         "</html>\n");
        if (n > 0) {
            html_len += (size_t)n;
        }
    }
    
    if (html_len > sizeof(html)) {
        html_len = sizeof(html);
    }
    send_response(client, 200, "OK", "text/html; charset=utf-8", html, html_len);
}

/* Add path to watch list */
static void add_watch_path(const char* path)
{
    struct stat st;

    if (watch_count >= MAX_WATCH_PATHS) {
        static int warned = 0;
        if (!warned) {
            fprintf(stderr,
                    "Warning: watch list full (%d paths), "
                    "some files will not trigger reload\n",
                    MAX_WATCH_PATHS);
            warned = 1;
        }
        return;
    }
    
    if (stat(path, &st) != 0) {
        return;
    }
    
    strncpy(watch_paths[watch_count].path, path, CXO_MAX_PATH - 1);
    watch_paths[watch_count].path[CXO_MAX_PATH - 1] = '\0';
    watch_paths[watch_count].mtime = st.st_mtime;
    watch_count++;
}

/* Scan directory and add to watch list */
static void scan_watch_dir(const char* dir)
{
    DIR* d;
    struct dirent* entry;
    char path[CXO_MAX_PATH];
    struct stat st;
    
    add_watch_path(dir);
    
    d = opendir(dir);
    if (!d) {
        return;
    }
    
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (stat(path, &st) != 0) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            scan_watch_dir(path);
        } else {
            add_watch_path(path);
        }
    }
    
    closedir(d);
}

/* Initialize file watching */
static void init_file_watching(void)
{
    watch_count = 0;
    
    if (cxo_access("content", F_OK) == 0) {
        scan_watch_dir("content");
    }
    
    if (cxo_access("themes", F_OK) == 0) {
        scan_watch_dir("themes");
    }
    
    add_watch_path("config.toml");
}

/* Check for file changes */
static int check_file_changes(void)
{
    int i;
    struct stat st;
    int changed = 0;
    
    for (i = 0; i < watch_count; i++) {
        if (stat(watch_paths[i].path, &st) != 0) {
            changed = 1;
        } else if (st.st_mtime != watch_paths[i].mtime) {
            watch_paths[i].mtime = st.st_mtime;
            changed = 1;
        }
    }
    
    return changed;
}

/* Run build in-process (no fork/PATH dependency) */
static int run_build(void)
{
    printf("\n[reload] Rebuilding...\n");

    if (cmd_build() == CXO_OK) {
        printf("[reload] Build complete\n\n");
        return 0;
    }
    printf("[reload] Build failed\n\n");
    return -1;
}

/* Create and bind server socket */
static cxo_socket_t setup_server_socket(int port)
{
    cxo_socket_t server_fd;
    struct sockaddr_in server_addr;
    int opt;
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == CXO_INVALID_SOCKET) {
        perror("socket");
        return -1;
    }
    
    opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        cxo_close_socket(server_fd);
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        cxo_close_socket(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        cxo_close_socket(server_fd);
        return -1;
    }
    
    return server_fd;
}

/* Set up signal handlers */
static void setup_signals(void)
{
#ifndef _WIN32
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#endif
}

/* Handle a single accepted client connection */
static void handle_client(int client_fd, sse_set_t* sse_clients)
{
    handle_request(client_fd, DEFAULT_ROOT, sse_clients);
    if (!sse_contains(sse_clients, client_fd)) {
        cxo_close_socket(client_fd);
    }
}

/* Check for file changes and trigger reload */
static void maybe_reload(sse_set_t* sse_clients, time_t* last_check)
{
    size_t i;
    time_t now;

    time(&now);
    if (now - *last_check < 1) {
        return;
    }
    *last_check = now;

    /* Prune disconnected SSE clients */
    for (i = sse_clients->count; i > 0; i--) {
        if (!check_sse_client(sse_clients->fds[i - 1])) {
            sse_remove_at(sse_clients, i - 1);
        }
    }

    if (check_file_changes()) {
        init_file_watching();
        if (run_build() == 0) {
            for (i = 0; i < sse_clients->count; i++) {
                send_reload_event(sse_clients->fds[i]);
            }
        }
    }
}

/* Main server select loop */
static void server_loop(cxo_socket_t server_fd, int rebuild,
                        sse_set_t* sse_clients)
{
    time_t last_check = 0;
    
    while (server_running) {
        fd_set readfds;
        struct timeval timeout;
        int ret;
        cxo_socket_t client_fd;
        struct sockaddr_in client_addr;
        cxo_socklen_t client_len;
        
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
        
        ret = select(server_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (ret < 0) {
            if (cxo_errno == CXO_EINTR) {
                continue;
            }
            perror("select");
            break;
        }
        
        if (rebuild) {
            maybe_reload(sse_clients, &last_check);
        }
        
        if (ret == 0) {
            continue;
        }
        
        if (!FD_ISSET(server_fd, &readfds)) {
            continue;
        }
        
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd == CXO_INVALID_SOCKET) {
            if (cxo_errno == CXO_EINTR || cxo_errno == CXO_EAGAIN || cxo_errno == CXO_EWOULDBLOCK) {
                continue;
            }
            perror("accept");
            continue;
        }
        
        handle_client(client_fd, sse_clients);
    }
}

/* Run development server */
int cmd_serve(int port, int rebuild)
{
    cxo_socket_t server_fd;
    struct stat st;
    sse_set_t sse_clients = { { 0 }, 0 };
    size_t i;
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "Error: WSAStartup failed\n");
        return CXO_ERR_IO;
    }
#endif
    
    if (stat(DEFAULT_ROOT, &st) != 0) {
        fprintf(stderr, "Error: %s/ directory not found\n", DEFAULT_ROOT);
        fprintf(stderr, "Run 'cxo build' first to generate the site.\n");
        return CXO_ERR_IO;
    }
    
    if (rebuild) {
        init_file_watching();
        cxo_setenv("CXO_HOTRELOAD", "1");
    }
    
    server_fd = setup_server_socket(port);
    if (server_fd == CXO_INVALID_SOCKET) {
        return CXO_ERR_IO;
    }
    
    setup_signals();
    
    printf("CXO development server running at http://localhost:%d\n", port);
    printf("Serving directory: %s/\n", DEFAULT_ROOT);
    if (rebuild) {
        printf("Hot reload: enabled (watching content/, themes/, config.toml)\n");
    }
    printf("Press Ctrl+C to stop\n\n");
    
    server_loop(server_fd, rebuild, &sse_clients);

    printf("\nShutting down server...\n");
    for (i = 0; i < sse_clients.count; i++) {
        cxo_close_socket(sse_clients.fds[i]);
    }
    cxo_close_socket(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    
    return CXO_OK;
}
