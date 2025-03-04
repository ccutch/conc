/** system.c - Example usage of the system module.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/


#define MEMORY_IMPLEMENTATION
#include "../source/1-memory.h"

#define RUNTIME_IMPLEMENTATION 
#include "../source/2-runtime.h"

#define SYSTEM_IMPLEMENTATION
#include "../source/3-system.h"


void run_command()
{
    // Run a command and capture its output
    SystemProc *proc = system_run("echo Hello, world! && echo Error message >&2");
    if (!proc) {
        fprintf(stderr, "Failed to start process\n");
        return;
    }

    // Read stdout from the process
    char stdout_buf[256] = {0};
    if (system_stdout(proc, stdout_buf, sizeof(stdout_buf)) > 0) {
        printf("STDOUT: %s\n", stdout_buf);
    } else {
        printf("Failed to read stdout\n");
    }

    // Read stderr from the process
    char stderr_buf[256] = {0};
    if (system_stderr(proc, stderr_buf, sizeof(stderr_buf)) > 0) {
        printf("STDERR: %s\n", stderr_buf);
    } else {
        printf("Failed to read stderr\n");
    }

    // Wait for the process to complete
    int exit_code = system_join(proc);
    printf("Process exited with code: %d\n", exit_code);
}


void get_environment_path()
{
    // Retrieve an environment variable
    char *path = system_getenv("PATH", "/usr/bin");
    printf("PATH environment variable: %s\n", path);
}


void write_and_read_a_file()
{
    // Remove file if it exists, we are ignoring errors
    char *file_path = "build/testfile.txt";
    system_remove(file_path);

    // Write content to the file that we just cleared
    char *message = "This is a test message.\n";
    int n;
    if ((n = system_write_file(file_path, message, 24)) > 0) {
        printf("File written successfully %d.\n", n);
    } else {
        printf("Failed to write file.\n");
    }

    // Read content from the file we just wrote to
    char file_buf[256] = {0};
    if ((n = system_read_file(file_path, file_buf, sizeof(file_buf))) > 0) {
        printf("File size: %d\n", n);
        printf("File contents: %s\n", file_buf);
    } else {
        printf("Failed to read file. %d\n", n);
    }
}


int main()
{
    runtime_run(run_command());
    runtime_run(get_environment_path());
    runtime_run(write_and_read_a_file());

    int res = runtime_main();
    if (res == 0) printf("✅ All tests passed!\n");
    return res;
}
