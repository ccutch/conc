/** runtime.h - Provides a cooperative runtime for our application.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-08
    @version  0.1.1 
    @license: MIT  
*/


#ifndef RUNTIME_HEADER
#define RUNTIME_HEADER


#include <unistd.h>


#define RUNTIME_FIBER_STACK_SIZE 8 * getpagesize()


typedef struct RuntimeFiber {

    // Fiber state for context switching
    void* stack_pointer;
    void* registers;

    // Memory tied to the lifetime of the fiber
    MemoryArena* memory;

} RuntimeFiber;


int runtime_main(void);

int runtime_id(void);

MemoryArena* runtime_memory(void);

void* runtime_alloc(int size);

int runtime_unblock_fd(int);

void runtime_start(void (*fn)(void*), void *arg);

void runtime_yield(void);

void runtime_read(int fd);

void runtime_write(int fd);

void runtime_resume(void* ptr);

void runtime_next(void);

void runtime_stop(void);

char* runtime_sprintf(char *, ...);

void runtime_logf(char *, ...);


#define runtime_run(fn) ({ \
    void __runtime_proc__(void) { fn; } \
    runtime_start((void*)__runtime_proc__, NULL); \
})


#ifdef RUNTIME_IMPLEMENTATION


#include <poll.h>
#include <stdarg.h>
#include <fcntl.h>


static int runtime_current_fiber = 0;
static MEMORY_SLICE(, RuntimeFiber) runtime_fibers  = {.count = 1};
static MEMORY_SLICE(, int) runtime_running_fibers   = {.count = 1};
static MEMORY_SLICE(, int) runtime_waiting_fibers   = {0};
static MEMORY_SLICE(, int) runtime_stopped_fibers   = {0};
static MEMORY_SLICE(, struct pollfd) runtime_polls  = {0};


int runtime_main(void)
{
    // Yield until all running or waiting fibers are finished
    while (runtime_running_fibers.count > 1 || runtime_waiting_fibers.count > 1)
        runtime_yield();
    return 0;
}


int runtime_id(void)
{
    // Dereference the current fiber from the list of running fibers
    return runtime_running_fibers.items[runtime_current_fiber];
}


MemoryArena* runtime_memory(void)
{
    return runtime_fibers.items[runtime_id()].memory;
}


void* runtime_alloc(int size)
{
    // Lazily initialize the first fiber if we are allocating memory globally
    if (runtime_fibers.count == 1 && runtime_fibers.capacity == 0) {
        runtime_fibers.items = malloc(sizeof(RuntimeFiber));
        runtime_fibers.items[0] = (RuntimeFiber){0};
        runtime_fibers.items[0].memory = memory_arena(MEMORY_DEFUALT_ARENA_SIZE);
        runtime_fibers.capacity = 1;
        runtime_running_fibers.items = malloc(sizeof(int));
        runtime_running_fibers.items[0] = 0;
        runtime_running_fibers.capacity = 1;
    }

    // Ensure that we have an arena for the current fiber
    RuntimeFiber* fiber = &runtime_fibers.items[runtime_id()];
    if (fiber->memory == NULL) {
        int capacity = MEMORY_DEFUALT_ARENA_SIZE > size ? MEMORY_DEFUALT_ARENA_SIZE : size;
        fiber->memory = memory_arena(capacity);
    }

    // Allocate memory using the Memory feature
    printf("runtime_alloc: %p\n", fiber->memory);
    printf("runtime_alloc: %d\n", size);
    return memory_alloc(fiber->memory, size);
}


int runtime_unblock_fd(int fd)
{
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}


void runtime_start(void (*fn)(void*), void *arg)
{
    int fiber_id;

    // Check stopped fibers for an available fiber id
    if (runtime_stopped_fibers.count > 0) {
        fiber_id = runtime_stopped_fibers.items[--runtime_stopped_fibers.count];
    } else {
        memory_slice_append(NULL, &runtime_fibers, ((RuntimeFiber){0}));
        fiber_id = runtime_fibers.count - 1;
        runtime_fibers.items[fiber_id].memory = memory_arena(MEMORY_DEFUALT_ARENA_SIZE);

        // Allocate stack with a guard page
        void *stack = mmap(NULL, RUNTIME_FIBER_STACK_SIZE + getpagesize(), PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (stack == MAP_FAILED) {
            perror("[ERROR] Failed to allocate coroutine stack");
            exit(1);
        }

        // Enable read/write permissions for the stack, leaving the first page protected
        mprotect(stack + getpagesize(), RUNTIME_FIBER_STACK_SIZE, PROT_READ | PROT_WRITE);
        runtime_fibers.items[fiber_id].registers = stack + getpagesize() + RUNTIME_FIBER_STACK_SIZE;
    }

    void** pointer = (void**)(runtime_fibers.items[fiber_id].registers - sizeof(void*));

    // Setting up the fiber stack
    *(--pointer) = runtime_stop;  // Return pointer
    *(--pointer) = fn;            // Function to execute
    *(--pointer) = arg;           // First argument (in %rdi)
    *(--pointer) = NULL;          // %rbp (Base pointer)
    *(--pointer) = NULL;          // %rbx
    *(--pointer) = NULL;          // %r12
    *(--pointer) = NULL;          // %r13
    *(--pointer) = NULL;          // %r14
    *(--pointer) = NULL;          // %r15

    // Set the stack pointer for the process
    runtime_fibers.items[fiber_id].stack_pointer = pointer;

    // Append to list of running fibers
    memory_slice_append(NULL, &runtime_running_fibers, fiber_id);
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
    int fiber_id = runtime_running_fibers.items[runtime_current_fiber++];
    runtime_fibers.items[fiber_id].stack_pointer = rsp;
    runtime_next();
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
    int fiber_id = runtime_running_fibers.items[runtime_current_fiber];
    runtime_fibers.items[fiber_id].stack_pointer = rsp;

    // Creating a pollfd for the file descriptor in read normal mode
    struct pollfd poll = {.fd = fd, .events = POLLRDNORM};
    memory_slice_append(NULL, &runtime_polls, poll);

    // We are not incrementing the current process like in
    // the yield function because this function is moved to the list
    // of waiting processes.
    memory_slice_append(NULL, &runtime_waiting_fibers, fiber_id);
    memory_slice_remove(&runtime_running_fibers, runtime_current_fiber);

    // Continue on to the next running process
    runtime_next();
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
    int fiber_id = runtime_running_fibers.items[runtime_current_fiber];
    runtime_fibers.items[fiber_id].stack_pointer = rsp;

    // Creating a pollfd for the file descriptor in write normal mode
    struct pollfd poll = {.fd = fd, .events = POLLWRNORM};
    memory_slice_append(NULL, &runtime_polls, poll);

    // We are not incrementing the current process like in
    // the yield function because this function is moved to the list
    // of waiting processes.
    memory_slice_append(NULL, &runtime_waiting_fibers, fiber_id);
    memory_slice_remove(&runtime_running_fibers, runtime_current_fiber);

    // Continue on to the next running process
    runtime_next();
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


void runtime_next(void)
{
    // Switch to the next running coroutine
    if (runtime_polls.count > 0) {
        // if there are multiple running procs we want to not block
        int timeout = runtime_running_fibers.count > 1 ? 0 : -1;
        int result = poll(runtime_polls.items, runtime_polls.count, timeout);
        if (result < 0) {
            runtime_logf("[ERROR] poll failed\n");
            exit(1);
        }

        // Wake up the first sleeping coroutine
        for (int i = 0; i < runtime_polls.count;) {
            if (runtime_polls.items[i].revents) {
                int proc_id = runtime_waiting_fibers.items[i];
                memory_slice_remove(&runtime_polls, i);
                memory_slice_remove(&runtime_waiting_fibers, i);
                memory_slice_append(NULL, &runtime_running_fibers, proc_id);
            } else { ++i; }
        }
    }

    // Ensure coroutines continue executing
    if (runtime_running_fibers.count > 0) {
        runtime_current_fiber %= runtime_running_fibers.count;
        runtime_resume(runtime_fibers.items[runtime_running_fibers.items[runtime_current_fiber]].stack_pointer);
    }
}


void runtime_stop(void)
{
    int fiber_id = runtime_running_fibers.items[runtime_current_fiber];
    if (fiber_id == 0) {
        runtime_logf("[ERROR] Main coroutine with id 0 should never finish\n");
        exit(1);
    }

    RuntimeFiber running_fiber = runtime_fibers.items[fiber_id];
    memory_empty(running_fiber.memory);

    memory_slice_append(NULL, &runtime_stopped_fibers, fiber_id);
    memory_slice_remove(&runtime_running_fibers, runtime_current_fiber);

    if (runtime_polls.count > 0) {
        int timeout = runtime_running_fibers.count > 1 ? 0 : -1;
        int result = poll(runtime_polls.items, runtime_polls.count, timeout);
        if (result < 0) {
            runtime_logf("[ERROR] poll failed\n");
            exit(1);
        }

        for (int i = 0; i < runtime_polls.count;)
            if (runtime_polls.items[i].revents) {
                int ctx = runtime_waiting_fibers.items[i];
                memory_slice_remove(&runtime_polls, i);
                memory_slice_remove(&runtime_waiting_fibers, i);
                memory_slice_append(NULL, &runtime_running_fibers, ctx);
            } else i++;
    }
    

    // Ensure we don't stop if there's at least one coroutine available
    if (runtime_running_fibers.count == 0 && runtime_waiting_fibers.count > 0) {
        memory_slice_append(NULL, &runtime_running_fibers, runtime_waiting_fibers.items[0]);
        memory_slice_remove(&runtime_waiting_fibers, 0);
        memory_slice_remove(&runtime_polls, 0);
    }

    if (runtime_running_fibers.count > 0) {
        runtime_current_fiber %= runtime_running_fibers.count;
        fiber_id = runtime_running_fibers.items[runtime_current_fiber];
        runtime_resume(runtime_fibers.items[fiber_id].stack_pointer);
    }
}


char* runtime_sprintf(char *fmt, ...)
{
    va_list args;

    // Count the size of the string to allocate memory
    va_start(args, fmt);
        int count = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    // Allocating memory in our current process arena
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

    // Allocating memory in our current process arena
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
