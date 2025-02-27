#ifndef APP_H
#define APP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_DB_ENTRIES 100
#define MAX_ROUTES 100
#define MAX_TEMPLATES 100
#define MAX_BODY_SIZE 1024
#define MAX_TEMPLATE_SIZE 4096
#define BUFFER_SIZE 4096

// Struct for Key-Value Store
typedef struct {
    char key[256];
    char value[1024];
} DBEntry;

// Simple Database
typedef struct {
    int count;
    DBEntry entries[MAX_DB_ENTRIES];
} SimpleDB;

// HTTP Request Object
typedef struct {
    int client_socket;
    FILE *response;
    char method[8];
    char path[256];
    char body[MAX_BODY_SIZE];
} Request;

// Template Structure
typedef struct {
    char name[256];
    char content[MAX_TEMPLATE_SIZE];
} Template;

// Dynamic Variable Callback Type
typedef void *(*VarCallback)(void *, Request *);

// Route Handler Type
typedef int (*HandlerFunc)(void *, Request *);

// Application Struct
typedef struct {
    SimpleDB db;
    Template templates[MAX_TEMPLATES];
    int template_count;
    struct {
        char path[256];
        char method[8];
        HandlerFunc handler;
    } routes[MAX_ROUTES];
    int route_count;
    struct {
        char key[256];
        VarCallback callback;
    } dynamic_vars[MAX_ROUTES];
    int var_count;
} App;

// Function Prototypes
App *new_application(const char *template_dir);
void app_serve(App *app, const char *route, const char *template_name);
void app_handler(App *app, const char *method, const char *route, HandlerFunc handler);
void app_var(const char *key, VarCallback callback);
int app_start(App *app, const char *host, int port);
void db_set(SimpleDB *db, const char *key, const char *value);
void db_get(SimpleDB *db, const char *key, char *out_value);
void *handle_request(void *arg);
char *render_template(Template *tpl, App *app, Request *req);

// Initialize Application
App *new_application(const char *template_dir) {
    App *app = (App *)malloc(sizeof(App));
    app->template_count = 0;
    app->route_count = 0;
    app->var_count = 0;
    app->db.count = 0;

    struct dirent *entry;
    DIR *d = opendir(template_dir);
    if (!d) {
        perror("Failed to open template directory");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(d)) != NULL && app->template_count < MAX_TEMPLATES) {
        if (entry->d_type == DT_REG) {
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "%s/%s", template_dir, entry->d_name);
            FILE *file = fopen(filepath, "r");
            if (!file) continue;

            fread(app->templates[app->template_count].content, 1, MAX_TEMPLATE_SIZE, file);
            fclose(file);

            strncpy(app->templates[app->template_count].name, entry->d_name, sizeof(app->templates[app->template_count].name) - 1);
            app->template_count++;
        }
    }
    closedir(d);

    return app;
}

// Register a Static Template Route
void app_serve(App *app, const char *route, const char *template_name) {
    app_handler(app, "GET", route, NULL);
    printf("Serving %s on route %s\n", template_name, route);
}

// Register a Dynamic Handler
void app_handler(App *app, const char *method, const char *route, HandlerFunc handler) {
    if (app->route_count >= MAX_ROUTES) {
        fprintf(stderr, "Route limit reached!\n");
        return;
    }
    strncpy(app->routes[app->route_count].method, method, sizeof(app->routes[app->route_count].method) - 1);
    strncpy(app->routes[app->route_count].path, route, sizeof(app->routes[app->route_count].path) - 1);
    app->routes[app->route_count].handler = handler;
    app->route_count++;
}

// Register a Dynamic Variable Callback
void app_var(const char *key, VarCallback callback) {
    printf("Registered dynamic variable: %s\n", key);
}

// Simple Key-Value Store (Set)
void db_set(SimpleDB *db, const char *key, const char *value) {
    for (int i = 0; i < db->count; i++) {
        if (strcmp(db->entries[i].key, key) == 0) {
            strncpy(db->entries[i].value, value, sizeof(db->entries[i].value) - 1);
            return;
        }
    }
    if (db->count < MAX_DB_ENTRIES) {
        strncpy(db->entries[db->count].key, key, sizeof(db->entries[db->count].key) - 1);
        strncpy(db->entries[db->count].value, value, sizeof(db->entries[db->count].value) - 1);
        db->count++;
    }
}

// Simple Key-Value Store (Get)
void db_get(SimpleDB *db, const char *key, char *out_value) {
    for (int i = 0; i < db->count; i++) {
        if (strcmp(db->entries[i].key, key) == 0) {
            strncpy(out_value, db->entries[i].value, 256);
            return;
        }
    }
    strncpy(out_value, "Not found", 256);
}

// Handle Incoming HTTP Requests
void *handle_request(void *arg) {
    Request *req = (Request *)arg;
    char buffer[BUFFER_SIZE];

    read(req->client_socket, buffer, sizeof(buffer));
    printf("Received request: %s\n", buffer);

    close(req->client_socket);
    free(req);
    return NULL;
}

// Start HTTP Server
int app_start(App *app, const char *host, int port) {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(host);
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server started on %s:%d\n", host, port);
    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        Request *req = (Request *)malloc(sizeof(Request));
        req->client_socket = client_socket;
        req->response = fdopen(client_socket, "w");

        pthread_t thread;
        pthread_create(&thread, NULL, handle_request, (void *)req);
        pthread_detach(thread);
    }
    return 1;
}

#endif // APP_H