/** network.c - Example usage of the network module.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/

#include <stdio.h>
#include <string.h>

#define MEMORY_IMPLEMENTATION
#include "../source/1-memory.h"

#define RUNTIME_IMPLEMENTATION 
#include "../source/2-runtime.h"

#define NETWORK_IMPLEMENTATION
#include "../source/4-network.h"


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
        int n = tcp_read_until(fd, buf, sizeof(buf) - 1, "\n");
        if (n <= 0) break;

        trim_whitespace(buf);
        if (strcmp(buf, "quit") == 0) break;

        strcat(buf, "\n");
        tcp_write(fd, buf, strlen(buf));
    }
    close(fd);
    exit(EXIT_SUCCESS);
}


int main(void)
{

    runtime_run(tcp_listen(9090, handler));

    return runtime_main();
}
