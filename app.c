#define APP_IMPLEMENTATION
#include "app.h"


void counter(void *arg)
{
    long int n = (long int)arg;
    for (int i = 0; i < n; ++i) {
        printf("[%lu] %d\n", runtime_id(), i);
        runtime_yield();
    }
}

void handler(void *arg)
{
    TcpConn *conn = (TcpConn *)arg;

    char buf[1024] = {0};

    while (true) {
        int n = tcp_read_until(conn->client_fd, buf, sizeof(buf) - 1, "\n"); // Read request
        if (n == 0) break; // Connection closed

        printf("Received: %s\n", buf); // Log the received request

        // Construct HTTP response
        tcp_write(conn->client_fd, buf, n); // Send responseb
    }
}

int main(void)
{
    runtime_init();
    runtime_run(counter, (void*)10);
    runtime_run(counter, (void*)20);

    nob_log(NOB_INFO, "Listening on localhost:9090");
    tcp_listen("127.0.0.1", 9090, handler);

    runtime_run_forever(); // Run forever
    return 0;
}


// int handle_name(App *app, Request *req)
// {
//     char name[100];
//     db_get(app->db, "name", name);

//     fprintf(req->response, "%s", name);
//     return 200;
// }

// int update_name(App *app, Request *req)
// {
//     char name[100];
//     fscan(req->body, "%s", name);
//     db_set(app->db, "name", name);

//     fprintf(req->response, "%s", name);
//     return 200;
// }

// typedef struct {
//     char name[100];
// } Profile;

// void *get_current_profile(App *app, Request *req)
// {
//     char name[100];
//     db_get(app->db, "name", name);

//     Profile *profile = {
//         .name = name,
//     };

//     return (void *)profile;
// }

// int main(void)
// {
//     App *app = new_application("./templates");

//     app_var("current_profile", get_current_profile);

//     app_serve(app, "/", "homepage.html");
//     app_serve(app, "/{profile}", "profile.html");
//     app_handler(app, "GET", "/name", handle_name);
//     app_handler(app, "PUT", "/name", update_name);

//     return app_start(app, "localhost", 8080);
// }