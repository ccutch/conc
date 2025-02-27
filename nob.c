#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    
    if (!nob_mkdir_if_not_exists("build")) return 1;

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "cc", "-Wall", "-Wextra", "-o", "build/app", "app.c");
    if (!nob_cmd_run_sync(cmd)) return 1;
    
    return 0;
}