#ifndef FILEIO_HEADER
#define FILEIO_HEADER

#include "runtime.h"

char *io_read_file(char *path);
int io_write_file(char *path, char *data, size_t size);
StrSlice io_read_dir(char *path);

#ifdef FILEIO_IMPLEMENTATION

// for implementation we want to use non blocking IO
// and call runtime_yield() when we have can wait,
// runtime_read() when we have data to read, and 
// runtime_write() when we have data to write.


#define FILEIO_READ_BUFFER_SIZE 1024


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>


char *io_read_file(char *path)
{
    size_t buffer_size = FILEIO_READ_BUFFER_SIZE;
    char *buffer = malloc(buffer_size);
    if (!buffer) return NULL;

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        free(buffer);
        return NULL;
    }

    size_t total_read = 0;
    while (true) {
        runtime_read(fd);
        ssize_t n = read(fd, buffer + total_read, FILEIO_READ_BUFFER_SIZE - total_read);
        if (n < 0) {  // This check is now correct
            free(buffer);
            return NULL;
        }
        total_read += n;
        if (total_read >= buffer_size) {
            buffer_size *= 2;
            buffer = realloc(buffer, buffer_size);
            if (!buffer) {
                close(fd);
                return NULL;
            }
        }
    }

    buffer[total_read] = '\0'; // Null-terminate
    close(fd);
    return buffer;
}


int io_write_file(char *path, char *data, size_t size)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) return -1;

    size_t total_written = 0;
    while (total_written < size) {
        runtime_write(fd);
        ssize_t n = write(fd, data + total_written, size - total_written);
        if (n < 0) {
            close(fd);
            return -1;
        }
        total_written += n;
    }

    close(fd);
    return 0;
}

StrSlice io_read_dir(char *path)
{
    StrSlice children = {0};
    DIR *dir = opendir(path);
    if (!dir) return children;

    errno = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        slice_append(&children, strdup(ent->d_name)); // Ensure strings are copied
        runtime_yield();
    }

    closedir(dir);
    return children; // Missing return statement fixed
}


#endif // FILEIO_IMPLEMENTATION
#endif // FILEIO_HEADER
