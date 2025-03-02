#ifndef SERVER_HEADER
#define SERVER_HEADER

#include "runtime.h"
#include "network.h"

typedef struct {
    char *path;
    char *method;
    StrSlice headers;
    char *body;
} HttpRequest;

typedef struct {
    int fd;
    int status;
    StrSlice headers;
    char *body;
} HttpResponse;


HttpRequest http_parse_request(int fd);

void http_handle(char *method, char *path, void (*handler)(HttpRequest, HttpResponse*));

void http_get_header(HttpRequest *req, char *key, char *value);

void http_get_body(HttpRequest *req, char *body);

void http_set_header(HttpResponse *res, char *key, char *value);

void http_set_body(HttpResponse *res, char *body);

void http_send(HttpResponse *res);

int http_serve(char *path, char *file_path);

int http_serve_dir(char *path, char *dir_path);

int http_listen_and_serve(int port);


#define SERVER_IMPLEMENTATION



#endif // SERVER_IMPLEMENTATION
#endif // SERVER_HEADER