
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

    // printf("req->content_length: %d\n", req->content_length);
    // char* buf = runtime_alloc(req->content_length);
    // network_read(req->conn_fd, buf, req->content_length);
    // printf("buf: %s\n", buf);
    // DataValue* value = encoding_from_json(buf);
    // printf("value: %s\n", data_to_string(value)->string);


    DataValue *response = data_dict(
        data_entry("hello", data_string("world")),
        data_entry("foo", data_string("bar")),
    NULL);

    // This call will check if the header has been written; if not, it writes:
    // HTTP/1.1 200 OK, plus headers, then the response.
    char* response_msg = encoding_to_json(response);
    printf("response_msg: %s\n", response_msg);
    network_write_body(req, response_msg, strlen(response_msg));
}


int main(void)
{
    runtime_run(network_listen(9090, handler));
    return runtime_main();
}
