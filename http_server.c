


#define RUNTIME_IMPLEMENTATION
#include "runtime.h"

#define NETWORK_IMPLEMENTATION
#include "network.h"

#define HTTP_IMPLEMENTATION
#include "http.h"


void handle_request(int fd)
{
    Request req = http_parse_request(fd);
    Response res = http_handle_request(req);

    
}

int main()
{
    tcp_listen(8080, handle_request);
    return 0;
}