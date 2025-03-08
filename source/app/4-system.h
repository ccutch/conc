/** system.h - Provides low level integration with the os for system calls,
              non-blocking file I/O, and environment variables.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-08
    @version  0.1.1
    @license: MIT
*/


#ifndef SYSTEM_HEADER
#define SYSTEM_HEADER


#include <stdbool.h>


// Process keeps pointers to a forked process
typedef struct SystemProcess {
    int pid;
    int stdout;
    int stderr;
} SystemProcess;


// System Process Functions

SystemProcess* system_exec(char* command);

int system_join(SystemProcess* process);

int system_kill(SystemProcess* process);

int system_stdout(SystemProcess* process, char* buffer, int size);

int system_stderr(SystemProcess* process, char* buffer, int size);


// System Environment Functions

char* system_getenv(char* name, char* def_value);


// Non-blocking I/O Functions

bool system_file_exists(char* path);

bool system_make_dir(char* path);

bool system_remove_dir(char* path);

bool system_remove_file(char* path);

int system_read_file(char* path, char* buffer, int size);

int system_write_file(char* path, char* buffer, int size);


#ifdef SYSTEM_IMPLEMENTATION


#include <strings.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>


SystemProcess* system_exec(char* command)
{
    int sysout[2];
    if (pipe(sysout) < 0) return NULL;

    int syserr[2];
    if (pipe(syserr) < 0) {
        close(sysout[0]);
        close(sysout[1]);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(sysout[0]);
        close(sysout[1]);
        close(syserr[0]);
        close(syserr[1]);
        return NULL;
    }

    if (pid == 0) {
        close(sysout[0]);
        close(syserr[0]);

        dup2(sysout[1], STDOUT_FILENO);
        dup2(syserr[1], STDERR_FILENO);

        close(sysout[1]);
        close(syserr[1]);

        execlp("sh", "sh", "-c", command, NULL);
        exit(1);
    }

    close(sysout[1]);
    close(syserr[1]);
    SystemProcess* process = runtime_alloc(sizeof(SystemProcess));
    process->pid = pid;
    process->stdout = sysout[0];
    process->stderr = syserr[0];
    return process;
}


int system_join(SystemProcess* process)
{
    int status;
    waitpid(process->pid, &status, 0);
    if (process->stdout > 0) close(process->stdout);
    if (process->stderr > 0) close(process->stderr);
    return status;
}


int system_kill(SystemProcess* process)
{
    if (kill(process->pid, SIGKILL) < 0) return -1;
    return system_join(process);
}


int system_stdout(SystemProcess* process, char* buffer, int size)
{
    int total_read = 0;
    while (total_read < size - 1) {
        int n = read(process->stdout, buffer + total_read, size - 1 - total_read);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(process->stdout);
                continue;
            }
            close(process->stdout);
            process->stdout = -1;
            return -1;
        } else if (n == 0) break;

        total_read += n;
    }

    close(process->stdout);
    process->stdout = -1;
    return total_read;
}


int system_stderr(SystemProcess* process, char* buffer, int size)
{
    int total_read = 0;
    while (total_read < size - 1) {
        int n = read(process->stderr, buffer + total_read, size - 1 - total_read);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(process->stderr);
                continue;
            }
            close(process->stderr);
            process->stderr = -1;
            return -1;
        } else if (n == 0) break;

        total_read += n;
    }

    close(process->stderr);
    process->stderr = -1;
    return total_read;
}


char* system_getenv(char* name, char* def_value)
{
    char* value = getenv(name);
    return value ? value : def_value;
}


bool system_file_exists(char* path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) < 0) {
        if (errno == ENOENT) return false;
        return false;
    }
    return true;
}


bool system_remove_file(char* path)
{
    return remove(path) < 0;
}


bool system_make_dir(char* path)
{
    return mkdir(path, 0755) < 0;
}


bool system_remove_dir(char* path)
{
    return rmdir(path) < 0;
}


int system_read_file(char* path, char* buffer, int size)
{
    int fd = open(path, O_NONBLOCK | O_RDONLY);
    if (fd < 0) return -1;

    int total_read = 0;
    while (total_read < size - 1) {
        int n = read(fd, buffer + total_read, size - 1 - total_read);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(fd);
                continue;
            }
            close(fd);
            return -1;
        } else if (n == 0) break;

        total_read += n;
    }

    close(fd);
    return total_read;
}


int system_write_file(char* path, char* buffer, int size)
{
    int fd = open(path, O_NONBLOCK | O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return -1;

    int total_written = 0;
    while (total_written < size) {
        int n = write(fd, buffer + total_written, size - total_written);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_write(fd);
                continue;
            }
            close(fd);
            return -1;
        }

        total_written += n;
    }
    close(fd);
    return total_written;
}


#endif // SYSTEM_IMPLEMENTATION
#endif // SYSTEM_HEADER
