
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
        perror("Failed to bind to socket");
        goto exit;
    }

    if (listen(fd, 128) < 0) {
        perror("Failed to listen on socket");
        goto exit;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    while (true) {
        runtime_read(fd);
        printf("Waiting for connection");

        socklen_t addr_len = sizeof(addr);
        int conn_fd = accept(fd, (struct sockaddr *)&addr, &addr_len);
        if (conn_fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue; 
            }
            perror("accept failed");
            continue;
        }

        flags = fcntl(conn_fd, F_GETFL, 0);
        if (flags >= 0) fcntl(conn_fd, F_SETFL, flags | O_NONBLOCK);

        runtime_start(handler, (void*)conn_fd);
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
            perror("tcp_read failed");
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
            perror("tcp_write failed");
            return -1;
        }
        total_written += n;
    }
    return total_written;
}


#endif // NETWORK_IMPLEMENTATION
#endif // NETWORK_HEADER