/** app.h - A framework for building high performance web apps.

    Overview
    ========

    This library provides a foundation for any of my future web apps.
    It features a cooperative runtime with non-blocking I/O, memory 
    management based on arena allocation, an HTML templating engine,
    and a database to store easily transferable data.


    Table of Contents:
    ===================

    Heading  ---- Provides a high level overview of the project and sets up a
                  convienent way to turn on all features of the library.

    Memory ------ Provides common data structures for storing data and the
                  implementation of our region based memory system.

    Runtime ----- Provides a cooperative runtime for our application for
                  managing memory and handling multiple processes at once.

    System ------ Provides low level integration with the os for system calls,
                  non-blocking file I/O, and environment variables.

    Network ----- Provides non-blocking tcp server, a basic HTTP interface, and
                  a path based router for handling incoming requests.

    Data -------- Provides a shallow layer of abstraction on top of json like 
                  data structures to allow for reflection and serialization.

    Database ---- Provides a wrapper around the sqlite3 library to store data in
                  a unstructured way with documents-based storage.

    Template ---- Provides a templating engine that will allow us to generate
                  HTML from our data structures, inspired by Go.
           
    Application - Provides a high level interface for starting our application,
                  binding data, serving files and folders.


    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-01
    @version  0.1.0
    @license: MIT
*/


#ifdef APP_IMPLEMENTATION
    #define MEMORY_IMPLEMENTATION
    #define RUNTIME_IMPLEMENTATION
    #define SYSTEM_IMPLEMENTATION
    #define NETWORK_IMPLEMENTATION
    #define DATA_IMPLEMENTATION
    #define DATABASE_IMPLEMENTATION
    #define TEMPLATE_IMPLEMENTATION
    #define APPLICATION_IMPLEMENTATION
#endif

/** memory.h - Provides a region based memory system for storing data.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-01
    @version  0.1.0 
    @license: MIT
*/


#ifndef MEMORY_HEADER
#define MEMORY_HEADER


#include <stddef.h>
#include <stdlib.h>


//  We need a dynamic way to store data about the current state of
//  our application and our actively running processes . Providing
//  a few common containers for storing data, using the name list.
//
//    .-----------------------------------------------------.
//    | <data type> * data_start | int capacity | int count |
//    '-----------------------------------------------------'


// Generic list of integer values, primarily used for storing
// the ids of processes in their different states
struct ListOfInt {
    int *items;
    int capacity;
    int count;
};


// Generic list of strings, primarily used for storing lists of
// null terminated strings
struct ListOfStr {
    char **items;
    int capacity;
    int count;
};


// appeds a new item to the list's memory after checking if the
// capacity is full. This will work generically for all lists.
#define list_append(list, item) ({ \
    if ((list)->count >= (list)->capacity) { \
        (list)->capacity += RUNTIME_LIST_SIZE; \
        (list)->items = realloc((list)->items, (list)->capacity * sizeof(item)); \
    } \
    (list)->items[(list)->count++] = (item); \
})


// removes an item from the list's memory and replaces it with
// the last item in the list, while also decrementing the count
#define list_remove(list, index) ({ \
    if ((index) >= (list)->count) { \
        perror("[ERROR] Index out of bounds\n"); \
        exit(1); \
    } \
    (list)->items[index] = (list)->items[--(list)->count]; \
})


#define MEMORY_REGION_SIZE 1 * getpagesize()


// Memory regions are used to store data in a way that will be all
// at the same time. This is useful for storing data that we do not
// want to be cleaned up after we have rendered a template.
typedef struct MemoryRegion {

    // The next region of memory after this capacity is used
    struct MemoryRegion *next;

    // The size of the region, named count to match other structs
    int count;

    // The capacity of the region, given when region is created
    int capacity;

    // A pointer to the memory that the region is using
    char *memory;

} MemoryRegion;


// Create a new region of memory with the given capacity, that will
// dynamically grow as needed, and all be cleaned up at once.
MemoryRegion *memory_new_region(int capacity);

// Allocate memory in the current region that will be freed later.
void *memory_alloc(MemoryRegion *region, int size);

// Get the total size of region and all of its children
int memory_size(MemoryRegion *region);

// Destroy a region of memory, freeing all memory associated with it.
void memory_destroy(MemoryRegion *region);


#ifdef MEMORY_IMPLEMENTATION


MemoryRegion *memory_new_region(int capacity)
{
    // Allocating memory for the region and failing gracefully
    MemoryRegion *region = malloc(sizeof(MemoryRegion));
    if (region == NULL) return NULL;

    // Allocating memory for the region
    region->memory = malloc(capacity);
    if (region->memory == NULL) {
        free(region);
        return NULL;
    }

    // Initialize remaining fields to zero
    region->capacity = capacity;
    region->count = 0;
    region->next = NULL;
    return region;
}


void *memory_alloc(MemoryRegion *region, int size)
{
    // Base case, just malloc if we are not provided a region
    if (region == NULL) return malloc(size);

    MemoryRegion *current = region;

    // Find a region that has enough capacity to store the memory
    while (current->next != NULL && current->count + size > current->capacity) {
        current = current->next;
    }

    // If we reached the end and still don't have enough capacity
    // we need to create a new region and add it to the last region
    if (current->count + size > current->capacity) {
        int capacity = (region->capacity > size) ? region->capacity : size;
        MemoryRegion *new_region = memory_new_region(capacity);
        if (new_region == NULL) return NULL;
        current->next = new_region;
        current = new_region;
    }

    // Allocating memory for the region
    void *memory = &current->memory[current->count];
    current->count += size;
    return memory;
}


int memory_size(MemoryRegion *region)
{
    int size = 0;
    while (region != NULL) {
        size += region->capacity;
        region = region->next;
    }
    return size;
}


void memory_destroy(MemoryRegion *region)
{
    // Recursively free all child regions 
    while (region != NULL) {
        MemoryRegion *next = region->next;
        free(region->memory);
        free(region);
        region = next;
    }
}


#endif // MEMORY_IMPLEMENTATION
#endif // MEMORY_HEADER

/** runtime.h - Provides a cooperative runtime for our application.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT  
*/


#ifndef RUNTIME_HEADER
#define RUNTIME_HEADER


#include <poll.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>


#define RUNTIME_LIST_SIZE 512 
#define RUNTIME_PROC_SIZE 8 * getpagesize()


// RuntimeProc is a cooperative process that will be scheduled
// and will clean up memory when it is done. This provides a
// backbone to our application server
struct RuntimeProc {

    // The stack pointer of the coroutine process
    void *stack_pointer;

    // A snapshot of the registers when the process
    // yields to another process
    void *registers;

    // Pointer to the memory region with references
    // to the front of the linked list of regions.
    struct MemoryRegion *memory;

};


// RuntimeProcs is a list of RuntimeProc that will be statically
// allocated and will be used to store the state of all processes
struct RuntimeProcs {
    struct RuntimeProc *items;
    int count;
    int capacity;
};


// RuntimePolls is a list of pollfd that will be statically allocated
// and will be used to store the pollfds of all processes
struct RuntimePolls {
    struct pollfd *items;
    int count;
    int capacity;
};


// Apply non-blocking mode to a file descriptor
int runtime_unblock_fd(int);


// Main function that will infinitely yield to any active process
int runtime_main(void);


// Return the id of the currently running runtime process
int runtime_id(void);


// Start a new process, this process can pass a single argument
void runtime_start(void (*main)(void*), void *arg);


// Yield the CPU to the next process waiting to use the resources
void runtime_yield(void);


// Suspend the current proccess until the fd is ready for reading
void runtime_read(int fd);


// Suspend the current proccess until the fd is ready for writing
void runtime_write(int fd);


// Continue onto the next process after yielding the resources
void runtime_continue(void);


// Resume execution of the coroutine by moving the pointer to %rsp
void runtime_resume(void* ptr);


// Finish the current process and resume the next active process
void runtime_finish(void);


// Allocate memory in the current process that will be freed later
void *runtime_alloc(int size);

// Return with a string allocated to current runtime memory region
char *runtime_sprint(char *, ...);

// Print a string to stderr using runtime allocation
void runtime_logf(char *, ...);


#ifdef __GNUC__

// Convenience macro for running a function in a new process
#define runtime_run(fn) ({ \
    void __runtime_proc__(void) { fn; } \
    runtime_start((void*)__runtime_proc__, NULL); \
})

#else

// This feature is only available for gcc due to the anonymous fn
#define runtime_run(fn) ({ \
    perror("[ERROR] runtime_run is only available for gcc\n"); \
    exit(1); \
})

#endif


#ifdef RUNTIME_IMPLEMENTATION


// Setting up static variables for global state of the runtime
static int runtime_current_proc = 0;

// Static buffer of pollfds for all waiting processes
static struct RuntimePolls runtime_polls = {0};

// Static buffer of all processes running or not
static struct RuntimeProcs runtime_all_procs = {.count = 1}; 

// Reference list of all actively running processes
static struct ListOfInt runtime_running_procs = {.count = 1};

// Reference list of all processes wainting for io
static struct ListOfInt runtime_waiting_procs = {0};

// Reference list of all processes that finished
static struct ListOfInt runtime_stopped_procs = {0};


int runtime_unblock_fd(int fd)
{
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}


int runtime_main(void)
{
    // I are checking both the running and waiting processes
    // to check that there are no processes that are running 
    while (runtime_running_procs.count > 1 || runtime_waiting_procs.count > 1)
        runtime_yield();
    return 0;
}


int runtime_id(void)
{
    // We use this function a lot to lookup the current proc.
    return runtime_running_procs.items[runtime_current_proc];
}

void runtime_start(void (*fn)(void*), void *arg) {
    int id;

    if (runtime_stopped_procs.count > 0) {
        // Reuse an stopped process ID if one is available
        id = runtime_stopped_procs.items[--runtime_stopped_procs.count];
    } else {
        // Create a new process and allocate memory region
        list_append(&runtime_all_procs, ((struct RuntimeProc){0}));
        id = runtime_all_procs.count - 1;
        runtime_all_procs.items[id].memory = memory_new_region(MEMORY_REGION_SIZE);

        // Allocate stack with a guard page
        void *stack = mmap(NULL, RUNTIME_PROC_SIZE + getpagesize(), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (stack == MAP_FAILED) {
            perror("[ERROR] Failed to allocate coroutine stack");
            exit(1);
        }

        // Enable read/write permissions for the stack, leaving the first page protected
        mprotect(stack + getpagesize(), RUNTIME_PROC_SIZE, PROT_READ | PROT_WRITE);
        runtime_all_procs.items[id].registers = stack + getpagesize() + RUNTIME_PROC_SIZE;
    }

    // Ensure stack pointer is within valid range
    void **pointer = (void**)(runtime_all_procs.items[id].registers - sizeof(void*));

    // Setting up the coroutine stack
    *(--pointer) = runtime_finish;  // Return pointer
    *(--pointer) = fn;              // Function to execute
    *(--pointer) = arg;             // First argument (in %rdi)
    *(--pointer) = NULL;            // %rbp (Base pointer)
    *(--pointer) = NULL;            // %rbx
    *(--pointer) = NULL;            // %r12
    *(--pointer) = NULL;            // %r13
    *(--pointer) = NULL;            // %r14
    *(--pointer) = NULL;            // %r15

    // Set the stack pointer for the process
    runtime_all_procs.items[id].stack_pointer = pointer;

    // Append to list of running processes
    list_append(&runtime_running_procs, id);
}


void __attribute__((naked)) runtime_yield(void)
{
    asm(
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rsp, %rdi\n"
    "    jmp _runtime_yield\n");
}


void _runtime_yield(void *rsp)
{
    // Pickup from jump in public facing function where the first arg
    // (aka %rdi) is the pointer to the return address that we can use
    // as the stack pointer for our current process. Then continue to
    // the next process.
    int running_id = runtime_running_procs.items[runtime_current_proc++];
    runtime_all_procs.items[running_id].stack_pointer = rsp;
    runtime_continue();
}


void __attribute__((naked)) runtime_read(int fd)
{
    (void)fd; asm(
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rdi, %rdi\n"
    "    movq %rsp, %rsi\n"
    "    jmp _runtime_read\n");
}


void _runtime_read(int fd, void *rsp)
{
    // Pickup from jump in public facing function where the first arg
    // (aka %rdi) is the file descriptor and the second arg (aka %rsi) 
    // is the pointer to the return address that we can use as the
    // stack pointer for our current process.
    int running_id = runtime_running_procs.items[runtime_current_proc];
    runtime_all_procs.items[running_id].stack_pointer = rsp;

    // Creating a pollfd for the file descriptor in read normal mode
    struct pollfd poll = {.fd = fd, .events = POLLRDNORM};
    list_append(&runtime_polls, poll);

    // We are not incrementing the current process like in
    // the yield function because this function is moved to the list
    // of waiting processes.
    list_append(&runtime_waiting_procs, running_id);
    list_remove(&runtime_running_procs, runtime_current_proc);

    // Continue on to the next running process
    runtime_continue();
}


void __attribute__((naked)) runtime_write(int fd)
{
    (void)fd; asm(
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rdi, %rdi\n"
    "    movq %rsp, %rsi\n"
    "    jmp _runtime_write\n");
}


void _runtime_write(int fd, void *rsp)
{
    // Pickup from jump in public facing function where the first arg
    // (aka %rdi) is the file descriptor and the second arg (aka %rsi) 
    // is the pointer to the return address that we can use as the
    // stack pointer for our current process.
    int running_id = runtime_running_procs.items[runtime_current_proc];
    runtime_all_procs.items[running_id].stack_pointer = rsp;

    // Creating a pollfd for the file descriptor in write normal mode
    struct pollfd poll = {.fd = fd, .events = POLLWRNORM};
    list_append(&runtime_polls, poll);

    // We are not incrementing the current process like in
    // the yield function because this function is moved to the list
    // of waiting processes.
    list_append(&runtime_waiting_procs, running_id);
    list_remove(&runtime_running_procs, runtime_current_proc);

    // Continue on to the next running process
    runtime_continue();
}


void runtime_resume(void* ptr)
{
    // This function is the inverse of the yield set of
    // functions. Rather than capturing the registers
    // we are interested in preserving, this function 
    // will restore the registers and return control of
    // to the current process's store stack pointer.
    (void)ptr; asm(
    "    movq %rdi, %rsp\n"
    "    popq %r15\n"
    "    popq %r14\n"
    "    popq %r13\n"
    "    popq %r12\n"
    "    popq %rbx\n"
    "    popq %rbp\n"
    "    popq %rdi\n"
    "    ret\n");
}


void runtime_continue(void)
{
    // Switch to the next running coroutine
    if (runtime_polls.count > 0) {
        // if there are multiple running procs we want to not block
        int timeout = runtime_running_procs.count > 1 ? 0 : -1;
        int result = poll(runtime_polls.items, runtime_polls.count, timeout);
        if (result < 0) {
            runtime_logf("[ERROR] poll failed\n");
            exit(1);
        }

        // Wake up the first sleeping coroutine
        for (int i = 0; i < runtime_polls.count;) {
            if (runtime_polls.items[i].revents) {
                int proc_id = runtime_waiting_procs.items[i];
                list_remove(&runtime_polls, i);
                list_remove(&runtime_waiting_procs, i);
                list_append(&runtime_running_procs, proc_id);
            } else { ++i; }
        }
    }

    // Ensure coroutines continue executing
    if (runtime_running_procs.count > 0) {
        runtime_current_proc %= runtime_running_procs.count;
        runtime_resume(runtime_all_procs.items[runtime_running_procs.items[runtime_current_proc]].stack_pointer);
    }
}


void runtime_finish(void)
{
    int running_id = runtime_running_procs.items[runtime_current_proc];
    if (running_id == 0) {
        runtime_logf("[ERROR] Main coroutine with id 0 should never finish\n");
        exit(1);
    }

    struct RuntimeProc running_proc = runtime_all_procs.items[running_id];
    MemoryRegion *region = running_proc.memory;

    // Cleaning up memory regions
    region = running_proc.memory;
    while ((region = running_proc.memory) != NULL)
        running_proc.memory = region->next;

    region = running_proc.memory;

    list_append(&runtime_stopped_procs, running_id);
    list_remove(&runtime_running_procs, runtime_current_proc);

    if (runtime_polls.count > 0) {
        int timeout = runtime_running_procs.count > 1 ? 0 : -1;
        int result = poll(runtime_polls.items, runtime_polls.count, timeout);
        if (result < 0) {
            runtime_logf("[ERROR] poll failed\n");
            exit(1);
        }

        for (int i = 0; i < runtime_polls.count;) {
            if (runtime_polls.items[i].revents) {
                int ctx = runtime_waiting_procs.items[i];
                list_remove(&runtime_polls, i);
                list_remove(&runtime_waiting_procs, i);
                list_append(&runtime_running_procs, ctx);
            } else { i++; }
        }
    }

    // Ensure we don't stop if there's at least one coroutine available
    if (runtime_running_procs.count == 0 && runtime_waiting_procs.count > 0) {
        list_append(&runtime_running_procs, runtime_waiting_procs.items[0]);
        list_remove(&runtime_waiting_procs, 0);
        list_remove(&runtime_polls, 0);
    }

    if (runtime_running_procs.count > 0) {
        runtime_current_proc %= runtime_running_procs.count;
        running_id = runtime_running_procs.items[runtime_current_proc];
        runtime_resume(runtime_all_procs.items[running_id].stack_pointer);
    }
}


void *runtime_alloc(int size)
{
    int process_id = runtime_running_procs.items[runtime_current_proc];
    struct RuntimeProc *running_proc = &runtime_all_procs.items[process_id];

    if (running_proc->memory == NULL) {
        int region_size = MEMORY_REGION_SIZE > size ? MEMORY_REGION_SIZE : size;
        running_proc->memory = memory_new_region(region_size);
        if (running_proc->memory == NULL) {
            runtime_logf("[ERROR] Failed to allocate memory for runtime process\n");
            exit(1);
        }
    }

    return memory_alloc(running_proc->memory, size);
}

char *runtime_sprint(char *fmt, ...)
{
    va_list args;

    // Count the size of the string to allocate memory
    va_start(args, fmt);
        int count = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    // Allocating memory in our current process region
    char *string = runtime_alloc(count + 1);
    if (string == NULL) return NULL;

    // Performing actual print opperation to the string
    va_start(args, fmt);
        vsnprintf(string, count + 1, fmt, args);
    va_end(args);

    return string;
}

void runtime_logf(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
        perror(runtime_sprint(fmt, args));
    va_end(args);
}



#endif // RUNTIME_IMPLEMENTATION
#endif // RUNTIME_HEADER
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
        runtime_logf("failed to execute shell command: %s\n", cmd);
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

            runtime_logf("failed to read stdout of process %d\n", proc->pid);
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

            runtime_logf("failed to read stderr of process %d\n", proc->pid);
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
        return -1;
    }
    return 1;
}


int system_remove(char * path)
{
    return remove(path) < 0;
}


int system_mkdir(char * path)
{
    return mkdir(path, 0755) < 0;
}


int system_rmdir(char * path)
{
    return rmdir(path) < 0;
}


int system_read_file(char * path, char * buf, int count)
{
    int fd = open(path, O_NONBLOCK | O_RDONLY);
    if (fd < 0) {
        runtime_logf("failed to open file: %s\n", path);
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

            runtime_logf("failed to read file %s", path);
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
        runtime_logf("failed to open file: %s\n", path);
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

            runtime_logf("failed to write file %s", path);
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

/** network.h - Provides non-blocking tcp server, a basic HTTP interface, and
                a path based router for handling incoming requests.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/


#ifndef NETWORK_HEADER
#define NETWORK_HEADER


// Start listening on given port for incoming TCP connections
void tcp_listen(int port, void (*handler)(int fd));

// Read data from TCP socket to buffer until size is reached
int tcp_read(int fd, char *buf, int size);

// Read data from TCP socket to buffer until delim is reached
int tcp_read_until(int fd, char *buf, int size, char *delim);

// Write data to TCP socket from buffer until size is reached
int tcp_write(int fd, char *buf, int size);


#ifdef NETWORK_IMPLEMENTATION
    

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>


void tcp_listen(int port, void (*handler)(int fd))
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0 && "Failed to create socket");

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port),
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        runtime_logf("Failed to bind to socket %d", port);
        goto exit;
    }

    if (listen(fd, 128) < 0) {
        runtime_logf("Failed to listen on socket");
        goto exit;
    }

    runtime_unblock_fd(fd);

    while (true) {
        runtime_read(fd);
        printf("Waiting for connection");

        socklen_t addr_len = sizeof(addr);
        int conn_fd = accept(fd, (struct sockaddr *)&addr, &addr_len);
        if (conn_fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue; 
            }
            runtime_logf("accept failed");
            continue;
        }

        runtime_unblock_fd(conn_fd);
        
        runtime_start((void*)handler, (void*)(long int)conn_fd);
        runtime_yield();
    }

exit:
    close(fd);
    return;
}


int tcp_read(int fd, char *buf, int size)
{
    return tcp_read_until(fd, buf, size, "\0");
}


int tcp_read_until(int fd, char *buf, int size, char *delim)
{
    int total_read = 0;

    while (total_read < size - 1) {
        int n = read(fd, buf + total_read, size - 1 - total_read);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_read(fd);
                continue;
            }

            runtime_logf("tcp_read failed");
            return -1;

        } else if (n == 0) return -1; 

        total_read += n;

        if (strstr(buf, delim)) {
            break;
        }
    }

    buf[total_read] = '\0';
    return total_read;
}


int tcp_write(int fd, char *buf, int size)
{
    int total_written = 0;

    while (total_written < size) {
        int n = write(fd, buf + total_written, size - total_written);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_write(fd);
                continue;
            }

            runtime_logf("tcp_write failed");
            return -1;
        }

        total_written += n;
    }

    return total_written;
}


#endif // NETWORK_IMPLEMENTATION
#endif // NETWORK_HEADER

/** data.h - Provides a shallow layer of abstraction on top of json like 
             data structures to allow for reflection and serialization.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/


#ifndef DATA_HEADER
#define DATA_HEADER


// TODO


#ifdef DATA_IMPLEMENTATION
    

// TODO


#endif // DATA_IMPLEMENTATION
#endif // DATA_HEADER

/** database.h - Provides a wrapper around the sqlite3 library to store data in
                 a unstructured way with documents-based storage.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/


#ifndef DATABASE_HEADER
#define DATABASE_HEADER


// TODO


#ifdef DATABASE_IMPLEMENTATION
    

// TODO


#endif // DATABASE_IMPLEMENTATION
#endif // DATABASE_HEADER

/** template.h - Provides a templating engine that will allow us to generate
                  HTML from our data structures, inspired by Go.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/


#ifndef TEMPLATE_HEADER
#define TEMPLATE_HEADER


// TODO


#ifdef TEMPLATE_IMPLEMENTATION
    

// TODO


#endif // TEMPLATE_IMPLEMENTATION
#endif // TEMPLATE_HEADER

/** application.h - Provides a high level interface for starting our application,
                    binding data, serving files and folders.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/


#ifndef APPLICATION_HEADER
#define APPLICATION_HEADER


#include <stdbool.h>


#define APP_DEFAULT_PORT 8080
#define APP_HANDLER _app_default_handler


struct ApplicationContext {
    struct DataList *context;
    struct FuncList {
        struct DataValue (*items)(struct NetworkRequest *req);
        int count;
        int capacity;
    } funcs;
};

// Handle HTTP requests with fine grain control of matching methods and paths
void app_handle(char *method, char *path, void (*handler)(struct NetworkRequest *req));

// Set a value in the application's context
void app_set(char *key, struct DataValue value);

// Save a function in the application's context
void app_func(char *key, struct DataValue (*func)(struct NetworkRequest *req));

// Serve a file after render with the application's context
void app_serve_file(char *path, char *file);

// Serve a directory, and optionally render the contents as templates
void app_serve_dir(char *dir, bool render);

// Start the application with a port
int app_start(int port);


#ifdef APPLICATION_IMPLEMENTATION


static struct ApplicationContext *default_context = {0};


void app_handle(char *method, char *path, void (*handler)(struct NetworkRequest *req))
{ runtime_logf("not implemented"); }


void app_set(char *key, struct DataValue value)
{ runtime_logf("not implemented"); }


void app_func(char *key, struct DataValue (*func)(struct NetworkRequest *req))
{ runtime_logf("not implemented"); }


void app_serve_file(char *path, char *file)
{ runtime_logf("not implemented"); }


void app_serve_dir(char *dir, bool render)
{ runtime_logf("not implemented"); }


void _app_default_handler(int fd)
{
    struct NetworkRequest *req = network_parse_http(fd);
    if (req == NULL) return;

    (void)req;
    // TODO: Handle request
    close(fd);
}


int app_start(int port)
{
    if (port == 0) port = APP_DEFAULT_PORT;
    runtime_start(network_listen(port, APP_HANDLER));
    return runtime_main();
}


#endif // APPLICATION_IMPLEMENTATION
#endif // APPLICATION_HEADER

