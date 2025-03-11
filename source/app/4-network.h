/** network.h - Provides a non-blocking TCP server, a basic HTTP interface,
    and a path-based router for handling incoming requests.

    Revised to correctly accept HTTP requests.
    @author:
    @date:    2025-03-08
    @version  0.1.2
    @license: MIT
*/


#ifndef NETWORK_HEADER
#define NETWORK_HEADER

typedef struct NetworkHeader {
    struct NetworkHeader* next;
    char *key;
    char *value;
} NetworkHeader;

typedef struct NetworkRequest {
    int conn_fd;
    char *method;
    char *path;
    NetworkHeader *req_headers;
    NetworkHeader *res_headers;
    int content_length;
    int res_status;
} NetworkRequest;

void network_listen_tcp(int port, void (*handler)(int));

void network_listen(int port, void (*handler)(NetworkRequest*));

int network_read(int fd, char *buf, int len);

int network_read_until(int fd, char *buf, int len, const char *delim);

int network_write(int fd, const char *buf, int len);

// or NULL if the request is malformed.
NetworkRequest* network_parse_http(int fd);

// Returns a header value from the incoming request headers.
char *network_get_header(NetworkRequest *req, const char *header);

// Sets a header in the response headers.
void network_set_header(NetworkRequest *req, const char *header, const char *value);

// Writes the HTTP response head to the connection. Once written, headers can no longer be modified.
int network_write_head(NetworkRequest *req, int status, const char *message);

// Writes a response body to the connection. If the head has not yet been written,
// this function writes a default 200 OK response head.
int network_write_body(NetworkRequest *req, const char *body, int len);


#ifdef NETWORK_IMPLEMENTATION


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>



void network_listen_tcp(int port, void (*handler)(int))
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        runtime_logf("failed to create socket on port %d", port);
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port),
    };

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        runtime_logf("failed to bind socket on port %d", port);
        close(server_fd);
        return;
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        runtime_logf("failed to listen on socket on port %d", port);
        close(server_fd);
        return;
    }

    runtime_unblock_fd(server_fd);
    printf("Listening on port %d\n", port);
    while (true) {
        socklen_t addr_size = sizeof(addr);
        int conn_fd = accept(server_fd, (struct sockaddr*)&addr, &addr_size);
        if (conn_fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(server_fd);
                continue;
            }
            runtime_logf("failed to accept connection on port %d", port);
            return;
        }
        runtime_unblock_fd(conn_fd);
        runtime_start((void*)handler, (void*)(long)conn_fd);
    }
}


void network_listen(int port, void (*handler)(NetworkRequest*))
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        runtime_logf("failed to create socket on port %d", port);
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port),
    };

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        runtime_logf("failed to bind socket on port %d", port);
        close(server_fd);
        return;
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        runtime_logf("failed to listen on socket on port %d", port);
        close(server_fd);
        return;
    }

    runtime_unblock_fd(server_fd);
    printf("Listening on port %d\n", port);
    while (true) {
        socklen_t addr_size = sizeof(addr);
        int conn_fd = accept(server_fd, (struct sockaddr*)&addr, &addr_size);
        if (conn_fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(server_fd);
                continue;
            }
            runtime_logf("failed to accept connection on port %d", port);
            return;
        }
        runtime_unblock_fd(conn_fd);
        NetworkRequest *req = network_parse_http(conn_fd);
        if (req == NULL) {
            close(conn_fd);
            continue;
        }
        runtime_start((void*)handler, (void*)req);
    }
}

// int network_read(int fd, char *buf, int size)
// {
//     return network_read_until(fd, buf, size, "\0");
// }


int network_read(int fd, char *buf, int n) {
    int total = 0;

    while (total < n) {
        printf("total: %d\n", total);
        printf("n: %d\n", n);
        int r = read(fd, buf + total, n - total);
        printf("r: %d\n", r);
        if (r < 0) {
            // If no data is available, only yield if we haven't read anything yet.
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                printf("ewouldblock: %d\n", EWOULDBLOCK);
                printf("eagain: %d\n", EAGAIN);
                printf("errno: %d\n", errno);
                printf("total: %d\n", total);
                if (total > 0) {
                    // We already have some data, so return what we got.
                    break;
                }
                // Otherwise, yield until more data is ready.
                runtime_read(fd);
                continue;
            }
            runtime_logf("failed to read from fd %d", fd);
            return -1;
        } else if (r == 0) {
            // Connection closed.
            break;
        }
        total += r;
    }
    buf[total] = '\0';
    return total;
}



int network_read_until(int fd, char *buf, int size, const char *delim)
{
    int total_read = 0;
    int delim_len = strlen(delim);

    while (total_read < size - 1) {
        int n = read(fd, buf + total_read, size - 1 - total_read);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(fd);
                continue;
            }
            runtime_logf("failed to read from fd %d", fd);
            return -1;
        } else if (n == 0) break;

        total_read += n;
        buf[total_read] = '\0';
        if (total_read >= delim_len && strstr(buf, delim) != NULL)
            break;
    }

    buf[total_read] = '\0';
    return total_read;
}


int network_write(int fd, const char *buf, int size)
{
    int total_written = 0;
    while (total_written < size) {
        int n = write(fd, buf + total_written, size - total_written);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_write(fd);
                continue;
            }
            runtime_logf("failed to write to fd %d", fd);
            return -1;
        }
        total_written += n;
    }
    return total_written;
}


static char* str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = runtime_alloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}


NetworkRequest* network_parse_http(int fd)
{
    char buf[2048] = {0};
    // Read until the end of the header block.
    int n = network_read_until(fd, buf, sizeof(buf) - 1, "\r\n\r\n");
    if (n <= 0) return NULL;

    char *line_save;
    char *line = strtok_r(buf, "\r\n", &line_save);
    if (!line) return NULL;

    char *req_save;
    char *method = strtok_r(line, " ", &req_save);
    char *path   = strtok_r(NULL, " ", &req_save);
    if (!method || !path) return NULL;

    NetworkRequest *req = runtime_alloc(sizeof(NetworkRequest));
    req->conn_fd = fd;
    req->method = str_dup(method);
    req->path   = str_dup(path);
    req->req_headers = NULL;
    req->res_headers = NULL;
    req->content_length = 0;
    req->res_status = 0;

    while ((line = strtok_r(NULL, "\r\n", &line_save)) != NULL) {
        if (strlen(line) == 0) break;

        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *key = line;
        char *value = colon + 1;
        while (*value == ' ') value++;

        NetworkHeader *header = runtime_alloc(sizeof(NetworkHeader));
        header->key = str_dup(key);
        header->value = str_dup(value);
        header->next = req->req_headers;
        req->req_headers = header;

        if (strcasecmp(header->key, "Content-Length") == 0)
            req->content_length = atoi(header->value);
    }

    return req;
}


char *network_get_header(NetworkRequest *req, const char *header)
{
    NetworkHeader *h = req->req_headers;
    while (h != NULL) {
        if (strcasecmp(h->key, header) == 0) return h->value;
        h = h->next;
    }
    return NULL;
}


void network_set_header(NetworkRequest *req, const char *header, const char *value)
{
    NetworkHeader *h = req->res_headers;
    while (h != NULL) {
        if (strcasecmp(h->key, header) == 0) {
            h->value = str_dup(value);
            return;
        }
        h = h->next;
    }

    NetworkHeader *new_header = runtime_alloc(sizeof(NetworkHeader));
    new_header->key = str_dup(header);
    new_header->value = str_dup(value);
    new_header->next = req->res_headers;
    req->res_headers = new_header;
}


int network_write_head(NetworkRequest *req, int status, const char *message)
{
    if (req->res_status != 0) return -1;
    req->res_status = status;

    int total_written = 0;
    char *status_line = runtime_sprintf("HTTP/1.0 %d %s\r\n", status, message);
    total_written += network_write(req->conn_fd, status_line, strlen(status_line));

    NetworkHeader *h = req->res_headers;
    while (h != NULL) {
        char *header_line = runtime_sprintf("%s: %s\r\n", h->key, h->value);
        total_written += network_write(req->conn_fd, header_line, strlen(header_line));
        h = h->next;
    }

    total_written += network_write(req->conn_fd, "\r\n", 2);
    return total_written;
}


int network_write_body(NetworkRequest *req, const char *body, int len)
{
    int total_written = 0;
    if (req->res_status == 0) {
        char length_str[16];
        snprintf(length_str, sizeof(length_str), "%d", len);
        network_set_header(req, "Content-Length", length_str);
        total_written = network_write_head(req, 200, "OK");
        if (total_written < 0) return -1;
    }


    total_written += network_write(req->conn_fd, body, len);
    close(req->conn_fd);
    req->conn_fd = -1;

    return total_written;
}

#endif // NETWORK_IMPLEMENTATION
#endif // NETWORK_HEADER
