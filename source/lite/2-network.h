/** network.h - Provides a non-blocking TCP server, a basic HTTP interface,
                and a path-based router for handling incoming requests.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-11
    @version  0.1.2
    @license: MIT
*/


#ifndef NETWORK_H
#define NETWORK_H


// Representation of an HTTP header with a link to the next header
typedef struct NetworkHeader {
    struct NetworkHeader* next;
    char *key;
    char *value;
} NetworkHeader;

// Representation of an HTTP request with inbound and outbound data
typedef struct NetworkRequest {
    int conn_fd;
    char *protocol;
    char *method;
    char *path;
    int req_length;
    int res_status;
    NetworkHeader *req_headers;
    NetworkHeader *res_headers;
} NetworkRequest;

// Endpoint handler for a request with a callback function
typedef struct NetworkEndpoint {
    const char *method;
    const char *path;
    void (*callback)(NetworkRequest *req);
} NetworkEndpoint;


// Network Router stores multiple endpoints and routes requests
typedef struct NetworkRouter {
    int count;
    int capacity;
    NetworkEndpoint **endpoints;
} NetworkRouter;


// Start network server with default network router
void network_get(const char *path, void (*callback)(NetworkRequest *req));

// void network_post(const char *method, const char *path, void (*callback)(NetworkRequest *req));

// void network_put(const char *method, const char *path, void (*callback)(NetworkRequest *req));

// void network_patch(const char *method, const char *path, void (*callback)(NetworkRequest *req));

// void network_delete(const char *method, const char *path, void (*callback)(NetworkRequest *req));

void network_listen(int port);

int network_read(int fd, char *buf, int len);

int network_read_until(int fd, char *buf, int len, const char *delim);

int network_write(int fd, const char *buf, int len);

char *network_get_header(NetworkRequest *req, const char *header);

void network_set_header(NetworkRequest *req, const char *header, const char *value);

int network_write_head(NetworkRequest *req, int status, const char *message);

int network_write_body(NetworkRequest *req, const char *body, int len);

void network_not_found(NetworkRequest*);


#ifdef NETWORK_IMPLEMENTATION


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdbool.h>


static NetworkRouter router = {
    .count = 0,
    .capacity = 0,
    .endpoints = NULL,
};


static NetworkEndpoint network_not_found_endpoint = {
    .method = "",
    .path = "",
    .callback = network_not_found,
};


static void _network_router_init(void)
{
    if (router.count >= router.capacity) {
        router.capacity += 10;
        router.endpoints = realloc(router.endpoints, sizeof(NetworkEndpoint) * router.capacity);
    }
}


void network_get(const char *path, void (*callback)(NetworkRequest *req))
{
    _network_router_init();
    NetworkEndpoint *endpoint = runtime_alloc(sizeof(NetworkEndpoint));
    endpoint->method = "GET";
    endpoint->path = path;
    endpoint->callback = callback;
    router.endpoints[router.count++] = endpoint;
}


static char* str_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = runtime_alloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}


static NetworkRequest* _network_parse_http(int fd)
{
    char buf[2048] = {0};
    // Read until the end of the header block.
    int n = network_read_until(fd, buf, sizeof(buf) - 1, "\r\n\r\n");
    if (n <= 0) return NULL;

    char *line_save;
    char *line = strtok_r(buf, "\r\n", &line_save);
    if (!line) return NULL;

    printf("Parsing: %s\n", line);

    char *req_save;
    char *method = strtok_r(line, " ", &req_save);
    char *path   = strtok_r(NULL, " ", &req_save);
    char *protocol = strtok_r(NULL, " ", &req_save);
    if (!method || !path) return NULL;

    printf("Handling: %s %s\n", method, path);
    NetworkRequest *req = runtime_alloc(sizeof(NetworkRequest));
    req->conn_fd = fd;
    req->protocol = str_dup(protocol);
    req->method = str_dup(method);
    req->path   = str_dup(path);
    req->req_headers = NULL;
    req->res_headers = NULL;
    req->req_length = 0;
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
            req->req_length = atoi(header->value);
    }

    return req;
}


static NetworkEndpoint *_network_lookup_endpoint(NetworkRequest *req)
{
    printf("Looking for endpoint for %s %s\n", req->method, req->path);
    for (int i = 0; i < router.count; i++) {
        NetworkEndpoint *endpoint = router.endpoints[i];
        printf("Testing endpoint %s %s\n", endpoint->method, endpoint->path);
        if (strcasecmp(endpoint->method, req->method) == 0 && strcasecmp(endpoint->path, req->path) == 0)
            return endpoint;
        printf("Testing Failed\n");
    }
    return &network_not_found_endpoint;
}


void network_listen(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("failed to create socket on port %d", port);
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
        printf("failed to bind socket on port %d", port);
        close(server_fd);
        return;
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        printf("failed to listen on socket on port %d", port);
        close(server_fd);
        return;
    }

    runtime_prepare(server_fd);

    printf("Listening on port %d\n", port);
    while (true) {
        runtime_reading(server_fd);
        socklen_t addr_size = sizeof(addr);
        int conn_fd = accept(server_fd, (struct sockaddr*)&addr, &addr_size);
        if (conn_fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue;
            }
            printf("failed to accept connection on port %d", port);
            return;
        }
        NetworkRequest *req = _network_parse_http(conn_fd);
        if (req == NULL) continue;
        NetworkEndpoint *endpoint = _network_lookup_endpoint(req);
        if (endpoint == NULL) continue;
        runtime_start((void*)endpoint->callback, (void*)req);
    }
}


int network_read(int fd, char *buf, int n) {
    int total = 0;

    while (total < n) {
        int r = read(fd, buf + total, n - total);
        if (r < 0) {
            // If no data is available, only yield if we haven't read anything yet.
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                if (total > 0) {
                    // We already have some data, so return what we got.
                    break;
                }
                // Otherwise, yield until more data is ready.
                runtime_reading(fd);
                continue;
            }
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
                runtime_reading(fd);
                continue;
            }
            printf("failed to read from fd %d", fd);
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
                runtime_writing(fd);
                continue;
            }
            printf("failed to write to fd %d", fd);
            return -1;
        }
        total_written += n;
    }
    return total_written;
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


void network_not_found(NetworkRequest *req)
{
    network_set_header(req, "Content-Type", "text/plain");
    const char *body = "not found";
    network_write_head(req, 404, "Not Found");
    network_write_body(req, body, strlen(body));
}


#endif // NETWORK_IMPLEMENTATION
#endif // NETWORK_H