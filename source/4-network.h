/** network.h - Provides non-blocking tcp server, a basic HTTP interface, and
                a path based router for handling incoming requests.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/


#ifndef NETWORK_HEADER
#define NETWORK_HEADER


void tcp_listen(int port, void (*handler)(int fd));
int tcp_read(int fd, char *buf, int size);
int tcp_read_until(int fd, char *buf, int size, char *end);
int tcp_write(int fd, char *buf, int size);


#ifdef NETWORK_IMPLEMENTATION
    

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>


void tcp_listen(int port, void (*handler)(int fd))
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0 && "Failed to create socket");

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port),
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to bind to socket %d", port);
        goto exit;
    }

    if (listen(fd, 128) < 0) {
        fprintf(stderr, "Failed to listen on socket");
        goto exit;
    }

    runtime_unblock_fd(fd);

    while (true) {
        runtime_read(fd);
        printf("Waiting for connection");

        socklen_t addr_len = sizeof(addr);
        int conn_fd = accept(fd, (struct sockaddr *)&addr, &addr_len);
        if (conn_fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue; 
            }
            fprintf(stderr, "accept failed");
            continue;
        }

        runtime_unblock_fd(conn_fd);
        
        runtime_start((void*)handler, (void*)(long int)conn_fd);
        runtime_yield();
    }

exit:
    close(fd);
    return;
}


int tcp_read(int fd, char *buf, int size)
{
    return tcp_read_until(fd, buf, size, "\0");
}


int tcp_read_until(int fd, char *buf, int size, char *end)
{
    int total_read = 0;
    while (total_read < size - 1) {
        int n = read(fd, buf + total_read, size - 1 - total_read);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(fd);
                continue;
            }
            fprintf(stderr, "tcp_read failed");
            return -1;
        } else if (n == 0) {
            return -1; 
        }
        total_read += n;

        if (strstr(buf, end)) {
            break;
        }
    }
    buf[total_read] = '\0';
    return total_read;
}


int tcp_write(int fd, char *buf, int size)
{
    int total_written = 0;
    while (total_written < size) {
        int n = write(fd, buf + total_written, size - total_written);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_write(fd);
                continue;
            }
            fprintf(stderr, "tcp_write failed");
            return -1;
        }
        total_written += n;
    }
    return total_written;
}


#endif // NETWORK_IMPLEMENTATION
#endif // NETWORK_HEADER