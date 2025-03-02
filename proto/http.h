#ifndef HTTP_HEADER
#define HTTP_HEADER

#include <stddef.h>

typedef struct {
    int conn_fd;
    char *method;
    char *path;
    StrSlice *headers;
} Request;

typedef struct {
    int conn_fd;
    int status;
    StrSlice *headers;
} Response;

Request *http_handle(int conn_fd);
char *http_get_body(Request *req);
char *http_get_header(Request *req, char *key);
void http_free_request(Request *req);

Response *http_response(Request *req);
void http_add_header(Response *res, char *key, char *value);
void http_write_head(Response *res, int status);
void http_write_body(Response *res, char *body);

#ifdef HTTP_IMPLEMENTATION


#include "runtime.h"
#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_MAX_HEAD_SIZE 1024
#define HTTP_MAX_BODY_SIZE 2048


Request *http_handle(int conn_fd)
{
    Request *req = malloc(sizeof(Request));
    if (!req) return NULL;

    req->conn_fd = conn_fd;
    req->headers = malloc(sizeof(StrSlice));
    if (!req->headers) {
        free(req);
        return NULL;
    }
    req->headers->count = 0;

    char desc[512];
    int n = tcp_read_until(conn_fd, desc, 512, "\r\n");
    if (n == -1) return req;

    sscanf(desc, "%ms %ms", &req->method, &req->path);
    if (!req->method || !req->path) return req;

    char buf[HTTP_MAX_HEAD_SIZE];
    n = tcp_read_until(conn_fd, buf, HTTP_MAX_HEAD_SIZE, "\r\n\r\n");
    if (n == -1) return req;

    // TODO parse headers line by line into the string slice

    return req;
}


char *http_get_header(Request *req, char *key)
{
    for (int i = 0; i < req->headers->count; i++) {
        char *line = strdup(req->headers->items[i]);
        char *value = strchr(line, ':');
        if (!value) continue;
        *value++ = '\0';
        
        while (*value == ' ') value++;

        if (strcmp(key, line) == 0) {
            char *result = strdup(value);
            free(line);
            return result;
        }

        free(line);
    }
    return NULL;
}


char *http_read_body(Request *req)
{
    char buf[HTTP_MAX_BODY_SIZE];
    int n = tcp_read(req->conn_fd, buf, HTTP_MAX_BODY_SIZE);
    if (n == -1) return NULL;
    return strdup(buf);
}


void http_free_request(Request *req)
{
    for (size_t i = 0; i < req->headers->count; i++)
        free(req->headers->items[i]);
    free(req->headers);
    free(req);
}


Response *http_response(Request *req)
{
    Response *res = malloc(sizeof(Response));
    res->conn_fd = req->conn_fd;
    return res;
}

void http_add_header(Response *res, char *key, char *value)
{
    if (res->status != 0) {
        printf("HTTP head already written\n");
        return;
    }

    slice_append(res->headers, key);
    slice_append(res->headers, ": ");
    slice_append(res->headers, value);
    slice_append(res->headers, "\r\n");
}

void http_write_head(Response *res, int status)
{
    if (res->status != 0) {
        printf("HTTP head already written\n");
        return;
    }

    res->status = status;

    tcp_write(res->conn_fd, "HTTP/1.1 ", strlen("HTTP/1.1 "));
    tcp_write(res->conn_fd, " OK\r\n", strlen(" OK\r\n"));
    for (size_t i = 0; i < res->headers->count; i++)
        tcp_write(res->conn_fd, res->headers->items[i], strlen(res->headers->items[i]));
    tcp_write(res->conn_fd, "\r\n", 2);
}

void http_write_body(Response *res, char *body)
{
    if (res->status == 0) {
        char *body_len = malloc(32);
        sprintf(body_len, "%ld", strlen(body));
        http_add_header(res, "Content-Type", "text/html");
        http_add_header(res, "Content-Length", body_len);
        http_write_head(res, 200);
    }
    dprintf(res->conn_fd, "%s", body);
}

#endif // HTTP_IMPLEMENTATION
#endif // HTTP_HEADER