/** runtime.h - Provides a cooperative runtime for our application.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-11
    @version  0.1.2 
    @license: MIT
*/


#ifndef RUNTIME_HEADER
#define RUNTIME_HEADER


#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>


#define RUNTIME_DEFAULT_ENGINE_CAPACITY 10

#define runtime_run(fn) ({ \
    void __fiber__(void) { fn; } \
    runtime_start((void*)__fiber__, NULL); \
})


// Who doesn't like unsigned ints?
typedef unsigned int uint;


// Tacking allocations to cleanup later
typedef struct RuntimeAllocation {
    uint size;
    void *start;
    struct RuntimeAllocation* next;
} RuntimeAllocation;


// Individual coroutine that manages memory
typedef struct RuntimeFiber {
    void *pointer;
    void *memory;
    bool waiting;
    struct pollfd poll;
    RuntimeAllocation *allocations;
} RuntimeFiber;


// Container for all fibers and counts
typedef struct RuntimeEngine {
    uint current;
    uint count;
    uint capacity;
    RuntimeFiber **fibers;
} RuntimeEngine;


// Memory related functions

uint runtume_memory_size(void);

void *runtime_alloc(uint size);

void *runtime_realloc(void *ptr, uint size);

void runtime_cleanup(void);


// Coroutine related functions

void runtime_start(void (*fn)(void*), void *arg);

void runtime_yield(void);

int runtime_prepare(int fd);

void runtime_reading(int fd);

void runtime_writing(int fd);

void runtime_resume(void* ptr);

void runtime_next(void);

void runtime_stop(void);

int runtime_main(void);


#ifdef RUNTIME_IMPLEMENTATION


#include <stdlib.h>
#include <fcntl.h>


// Globally allocated runtime engine for simpler API
static RuntimeEngine runtime = {
    .current = 0,
    .count = 1,
    .capacity = 0,
};


void _runtime_init(void)
{
    if (runtime.capacity == 0) {
        runtime.capacity += RUNTIME_DEFAULT_ENGINE_CAPACITY;
        runtime.fibers = malloc(sizeof(RuntimeFiber*) * runtime.capacity);
        runtime.fibers[runtime.current] = malloc(sizeof(RuntimeFiber));
        runtime.fibers[runtime.current]->pointer = NULL;
        runtime.fibers[runtime.current]->memory = malloc(getpagesize());
    } else if (runtime.count >= runtime.capacity) {
        runtime.capacity += RUNTIME_DEFAULT_ENGINE_CAPACITY;
        runtime.fibers = realloc(runtime.fibers, sizeof(RuntimeFiber*) * runtime.capacity);
    }
}


int runtime_memory_size(void)
{
    RuntimeFiber *fiber = runtime.fibers[runtime.current];
    if (fiber == NULL) return 0;
    uint size = 0;
    for (uint i = 0; i < runtime.count; i++) {
        RuntimeFiber *fiber = runtime.fibers[i];
        RuntimeAllocation *allocation = fiber->allocations;
        while (allocation != NULL) {
            size += allocation->size;
            allocation = allocation->next;
        }
    }
    return size;
}


void *runtime_alloc(uint size)
{
    _runtime_init();

    RuntimeAllocation *allocation = malloc(sizeof(RuntimeAllocation));
    if (allocation == NULL) return NULL;

    allocation->size = size;
    allocation->start = malloc(size);
    if (allocation->start == NULL) {
        free(allocation);
        return NULL;
    }

    RuntimeFiber *fiber = runtime.fibers[runtime.current];
    allocation->next = fiber->allocations;
    fiber->allocations = allocation;

    return allocation->start;
}


void *runtime_realloc(void *ptr, uint size)
{
    RuntimeFiber *fiber = runtime.fibers[runtime.current];
    RuntimeAllocation *allocation = fiber->allocations;
    while (allocation != NULL && allocation->start != ptr)
        allocation = allocation->next;

    if (allocation == NULL) return NULL;
    if (allocation->size == size) return ptr;
    if (allocation->size > size) return ptr;

   allocation->size = size;
   allocation->start = realloc(allocation->start, size);
   if (allocation->start == NULL) return NULL;

   return allocation->start;
}


void runtime_cleanup(void)
{
    if (runtime.capacity == 0) return;
    RuntimeFiber *fiber = runtime.fibers[runtime.current];
    while (fiber->allocations != NULL) {
        RuntimeAllocation *allocation = fiber->allocations;
        fiber->allocations = allocation->next;
        free(allocation->start);
        free(allocation);
    }
}


int runtime_prepare(int fd)
{ return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }


void runtime_start(void (*fn)(void*), void *arg)
{
    _runtime_init();

    RuntimeFiber *fiber = malloc(sizeof(RuntimeFiber));
    void *memory = malloc(getpagesize());
    fiber->memory = memory + getpagesize();
    fiber->waiting = false; 

    void **ptr = (void**)(fiber->memory - sizeof(void*));
    *(--ptr) = runtime_stop; // Return address
    *(--ptr) = fn;           // Function pointer 
    *(--ptr) = arg;          // %rdi
    *(--ptr) = NULL;         // %rbp
    *(--ptr) = NULL;         // %rbx
    *(--ptr) = NULL;         // %r12
    *(--ptr) = NULL;         // %r13
    *(--ptr) = NULL;         // %r14
    *(--ptr) = NULL;         // %r15
    fiber->pointer = ptr;    // save top of stack
    fiber->allocations = NULL;

    runtime.fibers[runtime.count++] = fiber;
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
    runtime.fibers[runtime.current++]->pointer = rsp;
    runtime_next();
}


void __attribute__((naked)) runtime_reading(int fd)
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
    "    jmp _runtime_reading\n");
}


void _runtime_reading(int fd, void *rsp)
{
    RuntimeFiber *fiber = runtime.fibers[runtime.current++];
    fiber->pointer = rsp;
    fiber->poll.fd = fd;
    fiber->poll.events = POLLRDNORM;
    fiber->waiting = true;
    runtime_next();
}


void __attribute__((naked)) runtime_writing(int fd)
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
    "    jmp _runtime_writing\n");
}


void _runtime_writing(int fd, void *rsp)
{
    RuntimeFiber *fiber = runtime.fibers[runtime.current++];
    fiber->pointer = rsp;
    fiber->poll.fd = fd;
    fiber->poll.events = POLLWRNORM;
    fiber->waiting = true;
    runtime_next();
}


void runtime_resume(void* ptr)
{
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
    runtime.current %= runtime.count;
    RuntimeFiber *fiber = runtime.fibers[runtime.current];

    if (fiber->waiting) {
        int timeout = runtime.count > 2 ? 0 : -1;
        int result = poll(&fiber->poll, 1, timeout);
        if (result < 0) {
            perror("[ERROR] poll failed\n");
            exit(1);
        }

        if (fiber->poll.revents) {
            fiber->poll = (struct pollfd){0};
            fiber->waiting = false;
            runtime_resume(fiber->pointer);
            return;
        }
    }

    runtime_resume(fiber->pointer);
}


void runtime_stop(void)
{
    RuntimeFiber *fiber = runtime.fibers[runtime.current];
    if (fiber == NULL) return;
    runtime.fibers[runtime.current] = runtime.fibers[--runtime.count];
    while (fiber->allocations != NULL) {
        RuntimeAllocation *allocation = fiber->allocations;
        fiber->allocations = allocation->next;
        free(allocation->start);
        free(allocation);
    }
    free(fiber->memory - getpagesize());
    free(fiber);
    runtime_next();
}


int runtime_main(void)
{
    while (runtime.count > 1) runtime_yield();
    free(runtime.fibers);
    return 0;    
}


#endif // RUNTIME_IMPLEMENTATION
#endif // RUNTIME_HEADER
