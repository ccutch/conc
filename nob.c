#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    
    if (!nob_mkdir_if_not_exists("build")) return 1;


    // TCP echo server
    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", "build/test", "test.c");
    if (!nob_cmd_run_sync(cmd)) return 1;

    // nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", "build/echo", "echo.c");
    // if (!nob_cmd_run_sync(cmd)) return 1;

    // nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", "build/proto", "proto.c");
    // if (!nob_cmd_run_sync(cmd)) return 1;

    // // HTTP file server
    // nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", "build/http", "app.c");
    // if (!nob_cmd_run_sync(cmd)) return 1;

    
    return 0;
}