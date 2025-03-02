
#define RUNTIME_IMPLEMENTATION
#include "proto/runtime.h"

#define HTTP_IMPLEMENTATION
#include "proto/http.h"

// #define HTML_IMPLEMENTATION
// #include "proto/html.h"

// #define DB_IMPLEMENTATION
// #include "proto/db.h"

// #define APP_IMPLEMENTATION
// #include "proto/app.h"

typedef struct {
    char *key;
    char *value;
} DataField;

typedef struct {
    DataField *fields;
    int n;
} ApiData;

void handle_api_data(Application *app, Request *req, Response *res)
{
    char buf[1024];
    int n = http_read(req, buf, 1024);
    if (n == -1) {
        http_write_header(res, 400);
        http_write_body(res, "Bad Request");
        return;
    }

    DbQuery query = {0};
    if (db_query_all(app->db, &query, "SELECT key, value FROM data") == -1) {
        http_write_header(res, 500);
        http_write_body(res, "Failed to find");
        return;
    }
    
    ApiData data = {0};
    for (int row = 0; row < n; row++) {
        DataField field = {0};
        if (db_scan(&query, &field.key, &field.value) == -1) {
            http_write_header(res, 500);
            http_write_body(res, "Failed to scan");
            return;
        }

        slice_append(data, field);
    }

    app_render(res, "api-data.html", &data);
}

int main(void)
{
    app_set("THEME", "dark");

    app_serve_dir("./public/");
    app_serve("/", "homepage.html");
    app_handle("GET", "/api/data", handle_api_data);

    app_start(8000);
}