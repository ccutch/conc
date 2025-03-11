/** test-network.c - Example usage of the network module.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/


#define MEMORY_IMPLEMENTATION
#include "../../source/app/1-memory.h"

#define RUNTIME_IMPLEMENTATION
#include "../../source/app/2-runtime.h"

#define SYSTEM_IMPLEMENTATION
#include "../../source/app/3-system.h"

#define NETWORK_IMPLEMENTATION
#include "../../source/app/4-network.h"


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
        if (strcmp(buf, "read") == 0) {
            char file_buf[256] = {0};
            if ((n = system_read_file("testfile.txt", file_buf, 256)) > 0) {
                network_write(fd, file_buf, n);
                continue;
            }
        }

        strcat(buf, "\n");
        network_write(fd, buf, strlen(buf));
    }

    close(fd);
    exit(EXIT_SUCCESS);
}


int main(void)
{
    // runtime_run(network_listen(9090, handler));
    return runtime_main();
}
