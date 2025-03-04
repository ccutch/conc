/** system.h - Provides low level integration with the os for system calls,
               non-blocking file I/O, and environment variables.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/


#ifndef SYSTEM_HEADER
#define SYSTEM_HEADER


typedef struct SystemProc {
    int out_fd;
    int err_fd;
    int pid;
} SystemProc;


// Run a new child process and return the file descriptor
SystemProc * system_run(char * cmd);

// Wait for a child process to finish and return the exit code
int system_join(SystemProc * proc);

// Kill a process by file descriptor, returning the exit code
int system_kill(SystemProc * proc);

// Read from stdout of a child process
int system_stdout(SystemProc * proc, char * buf, int count);

// Read from stderr of a child process
int system_stderr(SystemProc * proc, char * buf, int count);

// Get environment variable by name, or return default value
char * system_getenv(char * name, char * def_value);

// Check the existence of a file
int system_file_exists(char * path);

// Remove a file if it exists
int system_remove(char * path);

// Create a directory if it does not already exist
int system_mkdir(char * path);

// Remove a directory if it exists
int system_rmdir(char * path);

// Read from a file descriptor, file or child process
int system_read_file(char * path, char * buf, int count);

// Write to a file descriptor, file or child process
int system_write_file(char * path, char * buf, int count);


#ifdef SYSTEM_IMPLEMENTATION


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>


SystemProc * system_run(char * cmd) {
    int out_fd[2];
    if (pipe(out_fd) < 0) {
        perror("failed to create pipe");
        return NULL;
    }

    int err_fd[2];
    if (pipe(err_fd) < 0) {
        perror("failed to create pipe");
        return NULL;
    }

    int pid = fork();
    if (pid == -1) {
        perror("failed to create pipe");
        close(out_fd[0]);
        close(err_fd[0]);
        close(out_fd[1]);
        close(err_fd[1]);
        return NULL;
    }

    if (pid == 0) {
        // Child process
        close(out_fd[0]);
        close(err_fd[0]);

        dup2(out_fd[1], STDOUT_FILENO);
        dup2(err_fd[1], STDERR_FILENO);

        close(out_fd[1]);
        close(err_fd[1]);

        execl("/bin/sh", "sh", "-c", cmd, NULL);
        perror(runtime_sprint("failed to execute shell command: %s\n", cmd));
        exit(EXIT_FAILURE);
    }

    // Parent process
    close(out_fd[1]);
    close(err_fd[1]);

    runtime_unblock_fd(out_fd[0]);
    runtime_unblock_fd(err_fd[0]);

    SystemProc * proc = runtime_alloc(sizeof(SystemProc));
    proc->out_fd = out_fd[0];
    proc->err_fd = err_fd[0];
    proc->pid = pid;
    return proc;
}


int system_join(SystemProc * proc) {
    int status;
    if (waitpid(proc->pid, &status, 0) == -1) {
        perror("waitpid failed");
        free(proc);
        return -1;
    }

    if (proc->out_fd >= 0) close(proc->out_fd);
    if (proc->err_fd >= 0) close(proc->err_fd);

    free(proc);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
}


int system_kill(SystemProc * proc) {
    if (kill(proc->pid, SIGKILL) < 0) {
        perror("kill failed");
        return -1;
    }
    return system_join(proc);
}


int system_stdout(SystemProc * proc, char * buf, int count)
{
    int total_read = 0;
    while (total_read < count - 1) {
        int n = read(proc->out_fd, buf + total_read, count - 1 - total_read);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(proc->out_fd);
                continue;
            }

            perror(runtime_sprint("failed to read stdout of process %d\n", proc->pid));
            close(proc->out_fd);
            proc->out_fd = -1;
            return -1;
        } else if (n == 0) break;

        total_read += n;
    }

    close(proc->out_fd);
    proc->out_fd = -1;
    return total_read;
}


int system_stderr(SystemProc * proc, char * buf, int count)
{
    int total_read = 0;
    while (total_read < count - 1) {
        int n = read(proc->err_fd, buf + total_read, count - 1 - total_read);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(proc->err_fd);
                continue;
            }

            perror(runtime_sprint("failed to read stderr of process %d\n", proc->pid));
            close(proc->err_fd);
            proc->err_fd = -1;
            return -1;
        } else if (n == 0) break;

        total_read += n;
    }

    close(proc->err_fd);
    proc->err_fd = -1;
    return total_read;
}


char * system_getenv(char * name, char * def_value)
{
    char * value = getenv(name);
    return value ? value : def_value;
}


int system_file_exists(char * path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) < 0) {
        if (errno == ENOENT) return 0;
        perror(runtime_sprint("failed to check if file %s exists: %s", path, strerror(errno)));
        return -1;
    }
    return 1;
}


int system_remove(char * path)
{
    if (remove(path) < 0) {
        perror(runtime_sprint("failed to remove file: %s\n", path));
        return -1;
    }
    return 0;
}


int system_mkdir(char * path)
{
    if (mkdir(path, 0755) < 0) {
        perror(runtime_sprint("failed to create directory: %s\n", path));
        return -1;
    }
    return 0;
}


int system_rmdir(char * path)
{
    if (rmdir(path) < 0) {
        perror(runtime_sprint("failed to remove directory: %s\n", path));
        return -1;
    }
    return 0;
}


int system_read_file(char * path, char * buf, int count)
{
    int fd = open(path, O_NONBLOCK | O_RDONLY);
    if (fd < 0) {
        perror(runtime_sprint("failed to open file: %s\n", path));
        return -1;
    }

    int total_read = 0;
    while (total_read < count - 1) {
        int n = read(fd, buf + total_read, count - 1 - total_read);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(fd);
                continue;
            }

            perror(runtime_sprint("failed to read file %s", path));
            close(fd);
            return -1;
        } else if (n == 0) break;

        total_read += n;
    }

    close(fd);
    return total_read;
}


int system_write_file(char * path, char *buf, int count)
{
    int fd = open(path, O_NONBLOCK | O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror(runtime_sprint("failed to open file: %s\n", path));
        return -1;
    }

    int total_written = 0;
    while (total_written < count) {
        int n = write(fd, buf + total_written, count - total_written);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_write(fd);
                continue;
            }

            perror(runtime_sprint("failed to write file %s", path));
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