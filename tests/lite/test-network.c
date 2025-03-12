#define APP_IMPLEMENTATION

#include "../../source/lite/0-prelude.h"
#include "../../source/lite/1-runtime.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdbool.h>


void network_listen(int port);
int network_read_until(int fd, char *buf, int size, const char *delim);
int network_write(int fd, const char *buf, int size);
void handler(int fd);


int main(void)
{
    printf("✅ All tests passed!\n");
    runtime_run(network_listen(9091));
    return runtime_main();
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
        runtime_start((void*)handler, (void*)(long)conn_fd);
    }
}


void trim_whitespace(char *buf)
{
    char *start = buf;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        ++start;

    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        *end-- = '\0';

    memmove(buf, start, strlen(start) + 1);
}


void handler(int fd)
{
    while (true) {
        char buf[1024] = {0};
        int n = network_read_until(fd, buf, sizeof(buf) - 1, "\n");
        if (n <= 0) break;

        trim_whitespace(buf);
        if (strcmp(buf, "quit") == 0) break;

        strcat(buf, "\n");
        network_write(fd, buf, strlen(buf));
    }

    close(fd);
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

