#ifndef APPLICATION_HEADER
#define APPLICATION_HEADER


/////////////////
// APPLICATION //
/////////////////


#include <stdbool.h>

#define APP_DEFAULT_PORT 8080
#define APP_HANDLER _app_default_handler


struct ApplicationContext {
    struct DataList *context;
    struct FuncList {
        struct DataValue (*items)(struct NetworkRequest *req);
        int count;
        int capacity;
    } funcs;
};

// Handle HTTP requests with fine grain control of matching methods and paths
void app_handle(char *method, char *path, void (*handler)(struct NetworkRequest *req));

// Set a value in the application's context
void app_set(char *key, struct DataValue value);

// Save a function in the application's context
void app_func(char *key, struct DataValue (*func)(struct NetworkRequest *req));

// Serve a file after render with the application's context
void app_serve_file(char *path, char *file);

// Serve a directory, and optionally render the contents as templates
void app_serve_dir(char *dir, bool render);

// Start the application with a port
int app_start(int port);


#ifdef APPLICATION_IMPLEMENTATION


static struct ApplicationContext *default_context = {0};


void app_handle(char *method, char *path, void (*handler)(struct NetworkRequest *req))
{ fprintf(stderr, "not implemented"); }


void app_set(char *key, struct DataValue value)
{ fprintf(stderr, "not implemented"); }


void app_func(char *key, struct DataValue (*func)(struct NetworkRequest *req))
{ fprintf(stderr, "not implemented"); }


void app_serve_file(char *path, char *file)
{ fprintf(stderr, "not implemented"); }


void app_serve_dir(char *dir, bool render)
{ fprintf(stderr, "not implemented"); }


void _app_default_handler(int fd)
{
    struct NetworkRequest *req = network_parse_http(fd);
    if (req == NULL) return;

    (void)req;
    // TODO: Handle request
    close(fd);
}


int app_start(int port)
{
    if (port == 0) port = APP_DEFAULT_PORT;
    runtime_start(network_listen(port, APP_HANDLER))
    return runtime_main();
}


#endif // APPLICATION_IMPLEMENTATION
#endif // APPLICATION_HEADER