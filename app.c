#define APP_IMPLEMENTATION
#include "app.h"

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

int main(void)
{
    runtime_init();
    return 0;
    // App *app = new_application("./templates");

    // app_var("current_profile", get_current_profile);

    // app_serve(app, "/", "homepage.html");
    // app_serve(app, "/{profile}", "profile.html");
    // app_handler(app, "GET", "/name", handle_name);
    // app_handler(app, "PUT", "/name", update_name);

    // return app_start(app, "localhost", 8080);
}