
#define MEMORY_IMPLEMENTATION
#include "../source/app/1-memory.h"

#define RUNTIME_IMPLEMENTATION
#include "../source/app/2-runtime.h"

#define SYSTEM_IMPLEMENTATION
#include "../source/app/3-system.h"

#define NETWORK_IMPLEMENTATION
#include "../source/app/4-network.h"

#define DATA_IMPLEMENTATION
#include "../source/app/5-data-types.h"

#define ENCODING_IMPLEMENTATION
#include "../source/app/6-encoding.h"


void handler(NetworkRequest *req) {
    // Set a Content-Type header (and any others you need)
    network_set_header(req, "Content-Type", "text/plain");


    // This call will check if the header has been written; if not, it writes:
    // HTTP/1.1 200 OK, plus headers, then the body.
    char* response = encoding_to_json(
        data_dict(
            data_entry("hello", data_string("world")),
            data_entry("foo", data_string("bar")),
        NULL)
    );
    printf("response: %s\n", response);
    network_write_body(req, response, strlen(response));
}


int main(void)
{
    runtime_run(network_listen(9090, handler));
    return runtime_main();
}
