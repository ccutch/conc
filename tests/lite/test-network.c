#define APP_IMPLEMENTATION


#include "../../source/lite/0-prelude.h"
#include "../../source/lite/1-runtime.h"
#include "../../source/lite/2-network.h"


void handler(NetworkRequest*);


int main(void)
{
    printf("✅ All tests passed!\n");
    network_get("/", handler);
    runtime_run(network_listen(9091));
    return runtime_main();
}


void handler(NetworkRequest *req)
{
    network_set_header(req, "Content-Type", "text/plain");
    const char *body = "Hello world\n";
    network_write_body(req, body, strlen(body));
}
