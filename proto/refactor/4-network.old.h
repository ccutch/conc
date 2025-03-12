/** network.h - Provides a non-blocking tcp server, a basic HTTP interface, and
                a path based router for handling incoming requests.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-08
    @version  0.1.1 
    @license: MIT
*/


#ifndef NETWORK_HEADER
#define NETWORK_HEADER


// TCP based functions

// Start a TCP server on the given port, a new runtime fiber will
// handle the connection and will be scheduled when data is ready
void network_listen(int port, void (*handler)(int));

// Read data from the socket to the buffer until size is reached
int network_read(int fd, char *buf, int len);

// Read data from the socket to the buffer until delim is reached
int network_read_until(int fd, char *buf, int len, char *delim);

// Write data to the socket from the buffer until size is reached
int network_write(int fd, char *buf, int len);


typedef struct NetworkHeader {
    struct NetworkHeader* next;
    char *key;
    char *value;
} NetworkHeader;


// HTTP based types and functions
typedef struct NetworkRequest {
    int conn_fd;
    char *method;
    char *path;
    NetworkHeader *req_headers;
    NetworkHeader *res_headers;
    int content_length;
    int res_status;
} NetworkRequest;

// Populate a NetworkRequest object from data parsed from the socket
// Return NULL if the request is invalid or malformed
NetworkRequest* network_parse_http(int fd);

// Get a header from a NetworkRequest object from the user
char *network_get_header(NetworkRequest *req, char *header);

// Set a header in a NetworkRequest object to send to the user
void network_set_header(NetworkRequest *req, char *header, char *value);

// Start HTTP response to the conn_fd, once head is written to the socket
// headers can no longer be modified
int network_write_head(NetworkRequest *req, int status, char *message);

// Write body to the conn_fd, once head is written to the socket
// calling this function before write head will write a 200 OK
int network_write_body(NetworkRequest *req, char *body, int len);


#ifdef NETWORK_IMPLEMENTATION


#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>


void network_listen(int port, void (*handler)(int))
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
        socklen_t add_size = sizeof(addr);
        int conn_fd = accept(server_fd, (struct sockaddr*)&addr, &add_size);
        if (conn_fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(server_fd);
                continue;
            }
            runtime_logf("failed to accept connection on port %d", port);
            return;
        }
        runtime_unblock_fd(conn_fd);
        runtime_start((void*)handler, (void*)(long int)conn_fd);
    }
}


int network_read(int fd, char *buf, int size)
{
    return network_read_until(fd, buf, size, '\0');
}


int network_read_until(int fd, char *buf, int size, char *delim)
{
    int total_read = 0;

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
        if (strstr(buf, delim)) break;
    }

    buf[total_read] = '\0';
    return total_read;
}


int network_write(int fd, char *buf, int size)
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


NetworkRequest* network_parse_http(int fd)
{
    char buf[1024] = {0};
    int n = network_read_until(fd, buf, sizeof(buf) - 1, "\r\n");
    if (n <= 0) return NULL;

    char *first_line;
    strtok_r(buf, " ", &first_line);

    NetworkRequest *req = runtime_alloc(sizeof(NetworkRequest));
    req->conn_fd = fd;
    req->method = strtok_r(NULL, " ", &first_line);
    req->path = strtok_r(NULL, " ", &first_line);
    req->req_headers = NULL;
    req->res_headers = NULL;
    req->content_length = 0;
    req->res_status = 0;
    
    if (req->method == NULL
     || req->path   == NULL
     || strcmp(req->method, "") == 0
     || strcmp(req->path, "")   == 0
    ) return NULL;

    runtime_read(fd);

    n = network_read_until(fd, buf, sizeof(buf) - 1, "\n");
    if (n <= 0) return NULL;

    char *rest = buf;
    char *req_headers;
    char *header_line;
    
    while ((header_line = strtok_r(rest, "\r\n", &req_headers)) != NULL) {
        char *header_save;
        char *header_name = strtok_r(header_line, ": ", &header_save);
        char *header_value = strtok_r(NULL, "\r\n", &header_save);
        if (header_name == NULL || header_value == NULL) continue;

        NetworkHeader *header = runtime_alloc(sizeof(NetworkHeader));
        header->next = req->req_headers;
        header->key = header_name;
        header->value = header_value;

        req->req_headers = header;
        if (strcmp(header_name, "Content-Length") == 0)
            req->content_length = atoi(header_value);

        rest = NULL;
    }

    return req;
}


char *network_get_header(NetworkRequest *req, char *header)
{
    NetworkHeader *header_ptr = req->req_headers;
    while (header_ptr != NULL) {
        if (strcmp(header_ptr->key, header) == 0) return header_ptr->value;
        header_ptr = header_ptr->next;
    }
    return NULL;
}


void network_set_header(NetworkRequest *req, char *header, char *value)
{
    NetworkHeader *header_ptr = req->req_headers;
    while (header_ptr != NULL) {
        if (strcmp(header_ptr->key, header) == 0) {
            header_ptr->value = value;
            return;
        }
        header_ptr = header_ptr->next;
    }
    NetworkHeader *new_header = runtime_alloc(sizeof(NetworkHeader));
    new_header->next = req->req_headers;
    new_header->key = header;
    new_header->value = value;
    req->req_headers = new_header;
}


int network_write_head(NetworkRequest *req, int status, char *message)
{
    if (req->res_status != 0) return -1;
    req->res_status = status;

    int total_written = 0;
    char *status_str = runtime_sprintf("HTTP/1.1 %d %s\r\n", status, message);
    total_written += network_write(req->conn_fd, status_str, strlen(status_str));

    NetworkHeader *header_ptr = req->req_headers;
    while (header_ptr != NULL) {
        char *header_str = runtime_sprintf("%s: %s\r\n", header_ptr->key, header_ptr->value);
        total_written += network_write(req->conn_fd, header_str, strlen(header_str));
        header_ptr = header_ptr->next;
    }

    total_written += network_write(req->conn_fd, "\r\n", 2);
    return total_written;
}


int network_write_body(NetworkRequest *req, char *body, int len)
{
    int total_written = 0;
    if (req->res_status == 0) {
        network_set_header(req, "Content-Length", runtime_sprintf("%d", len));
        total_written += network_write_head(req, 200, "OK");
        if (total_written < 0) return -1;
    }
    total_written += network_write(req->conn_fd, body, len);
    return total_written;
}



#endif // NETWORK_IMPLEMENTATION
#endif // NETWORK_HEADER
