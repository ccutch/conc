#define RUNTIME_IMPLEMENTATION
#include "proto/runtime.h"

#define NETWORK_IMPLEMENTATION
#include "proto/network.h"


void trim_whitespace(char *buf)
{
    char *start = buf;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        ++start; // Move past leading spaces

    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
        *end-- = '\0'; // Remove trailing whitespace

    memmove(buf, start, strlen(start) + 1); // Shift buffer content to start
}


void handler(int fd)
{
    while (true) {
        char buf[1024] = {0};
        int n = tcp_read_until(fd, buf, sizeof(buf) - 1, "\n");
        if (n <= 0) break;

        trim_whitespace(buf);
        if (strcmp(buf, "quit") == 0) break;

        strcat(buf, "\n");
        tcp_write(fd, buf, strlen(buf));
    }
    close(fd);
}


void counter(int count)
{
    for (int i = 0; i <= count; i++) {
        printf("count to %d: %d\n", count, i);
        runtime_yield();
    }
}


int main(void)
{
    runtime_run(counter(10));
    runtime_run(counter(20));
    runtime_run(tcp_listen(9091, handler));
    runtime_main();
    return 0;
}
