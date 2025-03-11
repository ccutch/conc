
#define MEMORY_IMPLEMENTATION
#include "../source/app/1-memory.h"

#define RUNTIME_IMPLEMENTATION
#include "../source/app/2-runtime.h"

#define SYSTEM_IMPLEMENTATION
#include "../source/app/3-system.h"

#define NETWORK_IMPLEMENTATION
#include "../source/app/4-network.h"


void handler(NetworkRequest *req) {
    // Set a Content-Type header (and any others you need)
    network_set_header(req, "Content-Type", "text/plain");

    const char *body = "Hello, world!";
    // This call will check if the header has been written; if not, it writes:
    // HTTP/1.1 200 OK, plus headers, then the body.
    network_write_body(req, body, strlen(body));
}



int main(void)
{
    runtime_run(network_listen(9090, handler));
    return runtime_main();
}
