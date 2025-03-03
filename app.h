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

#ifndef MEMORY_HEADER
#define MEMORY_HEADER


////////////
// MEMORY //
////////////


#include <stddef.h>
#include <stdlib.h>


// We need a dynamic way to store data about the current state of
// our application and the processes that are running. Providing
// a few common containers for storing data, using the name list.


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
        fprintf(stderr, "[ERROR] Index out of bounds\n"); \
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

#ifndef RUNTIME_HEADER
#define RUNTIME_HEADER


/////////////
// RUNTIME //
/////////////


#include <poll.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>


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


#ifdef __GNUC__

// Convenience macro for running a function in a new process
#define runtime_run(fn) ({ \
    void __runtime_proc__(void) { fn; } \
    runtime_start((void*)__runtime_proc__, NULL); \
})

#else

// This feature is only available for gcc due to the anonymous fn
#define runtime_run(fn) ({ \
    fprintf(stderr, "[ERROR] runtime_run is only available for gcc\n"); \
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


void runtime_start(void (*fn)(void*), void *arg)
{
    // Obtaining an id for the new process in global state
    int id;

    // First checking if there are any stopped processed we
    // can take the id of, and if not we will create a new
    // process, with globally allocated memory for the stack
    if (runtime_stopped_procs.count > 0)
      id = runtime_stopped_procs.items[--runtime_stopped_procs.count];
    else {
        list_append(&runtime_all_procs, ((struct RuntimeProc){0}));
        id = runtime_all_procs.count - 1;
        runtime_all_procs.items[id].memory = memory_new_region(MEMORY_REGION_SIZE);
        runtime_all_procs.items[id].registers = mmap(NULL, RUNTIME_PROC_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK|MAP_GROWSDOWN, -1, 0);
        if (runtime_all_procs.items[id].registers == MAP_FAILED) {
            fprintf(stderr, "[ERROR] Failed to allocate memory for process stack\n");
            exit(1);
        }
    }

    // Setting up end of the stack to return to cleanup 
    // when the process finishes executing.
    void **pointer = (void**)((char*)runtime_all_procs.items[id].registers + RUNTIME_PROC_SIZE - sizeof(void*));
    *(--pointer) = runtime_finish; // return pointer
    *(--pointer) = fn;             // function pointer
    *(--pointer) = arg;            // %rdi (arg1)
    *(--pointer) = 0;              // %rbp
    *(--pointer) = 0;              // %rbx
    *(--pointer) = 0;              // %r12
    *(--pointer) = 0;              // %r13
    *(--pointer) = 0;              // %r14
    *(--pointer) = 0;              // %r15
    runtime_all_procs.items[id].stack_pointer = pointer;

    // Appending to the list of running processes
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
            fprintf(stderr, "[ERROR] poll failed\n");
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
        fprintf(stderr, "[ERROR] Main coroutine with id 0 should never finish\n");
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
            fprintf(stderr, "[ERROR] poll failed\n");
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
            fprintf(stderr, "[ERROR] Failed to allocate memory for runtime process\n");
            exit(1);
        }
    }

    return memory_alloc(running_proc->memory, size);
}



#endif // RUNTIME_IMPLEMENTATION
#endif // RUNTIME_HEADER
#ifndef SYSTEM_HEADER
#define SYSTEM_HEADER


////////////
// SYSTEM //
////////////


// TODO


#ifdef SYSTEM_IMPLEMENTATION
    

// TODO


#endif // SYSTEM_IMPLEMENTATION
#endif // SYSTEM_HEADER
#ifndef NETWORK_HEADER
#define NETWORK_HEADER


///////////
// NETWORK //
///////////


// TODO


#ifdef NETWORK_IMPLEMENTATION
    

// TODO


#endif // NETWORK_IMPLEMENTATION
#endif // NETWORK_HEADER
#ifndef DATA_HEADER
#define DATA_HEADER


//////////
// DATA //
//////////


// TODO


#ifdef DATA_IMPLEMENTATION
    

// TODO


#endif // DATA_IMPLEMENTATION
#endif // DATA_HEADER
#ifndef DATABASE_HEADER
#define DATABASE_HEADER


//////////////
// DATABASE //
//////////////


// TODO


#ifdef DATABASE_IMPLEMENTATION
    

// TODO


#endif // DATABASE_IMPLEMENTATION
#endif // DATABASE_HEADER
#ifndef TEMPLATE_HEADER
#define TEMPLATE_HEADER


//////////////
// TEMPLATE //
//////////////


// TODO


#ifdef TEMPLATE_IMPLEMENTATION
    

// TODO


#endif // TEMPLATE_IMPLEMENTATION
#endif // TEMPLATE_HEADER
#ifndef APPLICATION_HEADER
#define APPLICATION_HEADER


/////////////////
// APPLICATION //
/////////////////


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
{ fprintf(stderr, "not implemented"); }


void app_set(char *key, struct DataValue value)
{ fprintf(stderr, "not implemented"); }


void app_func(char *key, struct DataValue (*func)(struct NetworkRequest *req))
{ fprintf(stderr, "not implemented"); }


void app_serve_file(char *path, char *file)
{ fprintf(stderr, "not implemented"); }


void app_serve_dir(char *dir, bool render)
{ fprintf(stderr, "not implemented"); }


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
    runtime_start(network_listen(port, APP_HANDLER))
    return runtime_main();
}


#endif // APPLICATION_IMPLEMENTATION
#endif // APPLICATION_HEADER
