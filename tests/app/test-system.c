
#define MEMORY_IMPLEMENTATION
#include "../../source/app/1-memory.h"

#define RUNTIME_IMPLEMENTATION
#include "../../source/app/2-runtime.h"

#define SYSTEM_IMPLEMENTATION
#include "../../source/app/3-system.h"



void run_command()
{
    char buf[1024];

    // Run a command and capture its output
    SystemProcess *process = system_exec( "echo Hello, world! && echo Error message >&2");
    if (!process) {
        strcat(buf, "Failed to start process\n");
        return;
    }

    // Read stdout from the process
    char stdout_buf[256] = {0};
    if (system_stdout(process, stdout_buf, sizeof(stdout_buf)) > 0) {
        strcat(buf, runtime_sprintf("\nSTDOUT: %s", stdout_buf));
    } else {
        strcat(buf, "Failed to read stdout\n");
    }

    // Read stderr from the process
    char stderr_buf[256] = {0};
    if (system_stderr(process, stderr_buf, sizeof(stderr_buf)) > 0) {
        strcat(buf, runtime_sprintf("STDERR: %s", stderr_buf));
    } else {
        strcat(buf, "Failed to read stderr\n");
    }

    // Wait for the process to complete
    int exit_code = system_join(process);
    strcat(buf, runtime_sprintf("Exit code: %d\n", exit_code));

    printf("\n ======== Run CMD ======= \n%s\n", buf);
    fflush(stdout);
}


void get_environment_path()
{
    // Retrieve an environment variable
    char *path = system_getenv("PATH", "/usr/bin");
    printf("\n ======== Get ENV ======= \n\n$PATH: %s\n", path);
    fflush(stdout);
}


void write_and_read_a_file()
{
    char buf[1024];

    // Remove file if it exists, we are ignoring errors
    char *file_path = "build/testfile.txt";
    system_remove_file(file_path);

    // Write content to the file that we just cleared
    char *message = "This is a test message.\n";
    int n;
    if ((n = system_write_file(file_path, message, 24)) > 0) {
        strcat(buf, runtime_sprintf("File written successfully %d bytes.\n", n));
    } else {
        strcat(buf, "Failed to write file.\n");
    }

    // Read content from the file we just wrote to
    char file_buf[256] = {0};
    if ((n = system_read_file(file_path, file_buf, sizeof(file_buf))) > 0) {
        strcat(buf, runtime_sprintf("File read successfully %d bytes.\n", n));
        strcat(buf, runtime_sprintf("File contents: %s", file_buf));
    } else {
        strcat(buf, runtime_sprintf("Failed to read file. %d\n", n));
    }

    printf("\n ======== File I/O ======= \n\n%s", buf);
    fflush(stdout);
}


int main()
{
    // executes fastest
    runtime_run(write_and_read_a_file());

    // executes middle
    runtime_run(get_environment_path());

    // executes slowest
    runtime_run(run_command());

    int res = runtime_main();
    if (res == 0) printf("✅ All tests passed!\n");
    return res;
}
