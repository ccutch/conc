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
char *runtime_sprintf(char *, ...);

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
static struct IntSlice runtime_running_procs = {.count = 1};

// Reference list of all processes wainting for io
static struct IntSlice runtime_waiting_procs = {0};

// Reference list of all processes that finished
static struct IntSlice runtime_stopped_procs = {0};


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
        slice_append(&runtime_all_procs, ((struct RuntimeProc){0}));
        id = runtime_all_procs.count - 1;
        runtime_all_procs.items[id].memory = memory_new_region(MEMORY_DEFAULT_REGION_SIZE);

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
    slice_append(&runtime_running_procs, id);
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
    slice_append(&runtime_polls, poll);

    // We are not incrementing the current process like in
    // the yield function because this function is moved to the list
    // of waiting processes.
    slice_append(&runtime_waiting_procs, running_id);
    slice_remove(&runtime_running_procs, runtime_current_proc);

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
    slice_append(&runtime_polls, poll);

    // We are not incrementing the current process like in
    // the yield function because this function is moved to the list
    // of waiting processes.
    slice_append(&runtime_waiting_procs, running_id);
    slice_remove(&runtime_running_procs, runtime_current_proc);

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
                slice_remove(&runtime_polls, i);
                slice_remove(&runtime_waiting_procs, i);
                slice_append(&runtime_running_procs, proc_id);
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

    slice_append(&runtime_stopped_procs, running_id);
    slice_remove(&runtime_running_procs, runtime_current_proc);

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
                slice_remove(&runtime_polls, i);
                slice_remove(&runtime_waiting_procs, i);
                slice_append(&runtime_running_procs, ctx);
            } else { i++; }
        }
    }

    // Ensure we don't stop if there's at least one coroutine available
    if (runtime_running_procs.count == 0 && runtime_waiting_procs.count > 0) {
        slice_append(&runtime_running_procs, runtime_waiting_procs.items[0]);
        slice_remove(&runtime_waiting_procs, 0);
        slice_remove(&runtime_polls, 0);
    }

    if (runtime_running_procs.count > 0) {
        runtime_current_proc %= runtime_running_procs.count;
        running_id = runtime_running_procs.items[runtime_current_proc];
        runtime_resume(runtime_all_procs.items[running_id].stack_pointer);
    }
}


void *runtime_alloc(int size)
{
    fprintf(stderr, "Allocating %d bytes\n", size);
    fflush(stderr);

    // Lazy initialization of the first process
    // since we are not always using it.
    if (runtime_running_procs.count == 1 && runtime_all_procs.capacity == 0) {
        fprintf(stderr, "Initializing zeroth process\n");
        fflush(stderr);
        runtime_all_procs.items = malloc(sizeof(struct RuntimeProc));
        runtime_all_procs.items[0] = (struct RuntimeProc){0};
        runtime_all_procs.items[0].memory = memory_new_region(MEMORY_DEFAULT_REGION_SIZE);
        runtime_all_procs.capacity = 1;
        runtime_running_procs.items = malloc(sizeof(int));
        runtime_running_procs.items[0] = 0;
        runtime_running_procs.capacity = 1;
    }

    int process_id = runtime_running_procs.items[runtime_current_proc];
    struct RuntimeProc *running_proc = &runtime_all_procs.items[process_id];

    if (running_proc->memory == NULL) {
        int region_size = MEMORY_DEFAULT_REGION_SIZE > size ? MEMORY_DEFAULT_REGION_SIZE : size;
        fprintf(stderr, "Allocating %d bytes in process %d\n", region_size, process_id);
        fflush(stderr);
        running_proc->memory = memory_new_region(region_size);
        if (running_proc->memory == NULL) {
            fprintf(stderr, "[ERROR] Failed to allocate memory for runtime process\n");
            fflush(stderr);
            exit(1);
        }
    }

    return memory_alloc(running_proc->memory, size);
}

char *runtime_sprintf(char *fmt, ...)
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

    // Count the size of the string to allocate memory
    va_start(args, fmt);
        int count = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    // Allocating memory in our current process region
    char *string = runtime_alloc(count + 1);
    if (string == NULL) return;

    // Performing actual print opperation to the string
    va_start(args, fmt);
        vsnprintf(string, count + 1, fmt, args);
    va_end(args);

    fprintf(stderr, "%s", string);
    fflush(stderr);
}



#endif // RUNTIME_IMPLEMENTATION
#endif // RUNTIME_HEADER