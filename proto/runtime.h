#ifndef RUNTIME_HEADER

#define RUNTIME_HEADER

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>










// Helpers for working with slices of data

#define SLICE_DEFAULT_CAPACITY 256

#define slice_append(slice, item) \
    do { \
        if ((slice)->count >= (slice)->capacity) { \
            (slice)->capacity = (slice)->capacity == 0 ? SLICE_DEFAULT_CAPACITY : (slice)->capacity*2; \
            (slice)->items = realloc((slice)->items, sizeof(item) * (slice)->capacity); \
        } \
        (slice)->items[(slice)->count++] = (item); \
    } while (0);

#define slice_remove(slice, i) \
    do { \
        size_t j = i; \
        assert(j < (slice)->count && "Index out of bounds"); \
        (slice)->items[i] = (slice)->items[--(slice)->count]; \
    } while (0);


// Example of generic slices

typedef struct {
    size_t *items;
    size_t count;
    size_t capacity;
} NumSlice;

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} StrSlice;

typedef struct {
    void **items;
    size_t count;
    size_t capacity;
} GenSlice;









// Asynchronous runtime to run coroutines

typedef struct {
    void *stack_ptr;
    void *registers;
} RuntimeContext;

typedef struct {
    RuntimeContext *items;
    size_t count;
    size_t capacity;
} RuntimeContextSlice;

typedef struct {
    struct pollfd *items;
    size_t count;
    size_t capacity;
} RuntimePollSlice;


// Run until all coroutines are stopped
void runtime_main(void);

// Return the id of the current coroutine
size_t runtime_id(void);

// Start a new coroutine
void runtime_start(void (*fn)(void*), void *arg);

// Yield control to the next coroutine
void runtime_yield(void);

// Yield control and register pollin
void runtime_read(int);

// Yield control and register pollout 
void runtime_write(int);

// Restore the stack pointer of the current coroutine
void runtime_restore(void *ptr);

// Wake up next sleeping coroutine and activate
void runtime_continue(void);

// Finish the current context and move to inactive
void runtime_finish_current(void);


#ifdef RUNTIME_IMPLEMENTATION

#define STACK_SIZE (1024 * 4)

static size_t current_ctx = 0;
static RuntimeContextSlice contexts = {.count = 1};
static RuntimePollSlice polls = {0};
static NumSlice running = {.count = 1};
static NumSlice waiting = {0};
static NumSlice stopped = {0};


void runtime_start(void (*fn)(void*), void *arg)
{
    size_t id;
    if (stopped.count > 0) {
        id = stopped.items[--stopped.count];
    } else {
        slice_append(&contexts, ((RuntimeContext){0}));
        id = contexts.count - 1;
        contexts.items[id].registers = mmap(NULL, STACK_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK|MAP_GROWSDOWN, -1, 0);
        assert(contexts.items[id].registers != MAP_FAILED);
    }

    void **pointer = (void**)((char*)contexts.items[id].registers + STACK_SIZE - sizeof(void*));
    *(--pointer) = runtime_finish_current;
    *(--pointer) = fn;
    *(--pointer) = arg;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    contexts.items[id].stack_ptr = pointer;

    slice_append(&running, id);
}

#define runtime_run(fn) \
    do { \
      void __fn__(void) { fn; } \
      runtime_start((void*)__fn__, NULL); \
    } while (0);


size_t runtime_id(void)
{
    return running.items[current_ctx];
}

void runtime_main(void)
{
    while (running.count > 1 || waiting.count > 1)
        runtime_yield();
}



void __attribute__((naked)) runtime_restore(void *rsp)
{
    (void)rsp; asm(
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
    contexts.items[running.items[current_ctx++]].stack_ptr = rsp;
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
    contexts.items[running.items[current_ctx]].stack_ptr = rsp;
    struct pollfd poll = {.fd = fd, .events = POLLRDNORM};
    slice_append(&waiting, running.items[current_ctx]);
    slice_append(&polls, poll);
    slice_remove(&running, current_ctx);
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
    contexts.items[running.items[current_ctx]].stack_ptr = rsp;
    struct pollfd poll = {.fd = fd, .events = POLLWRNORM};
    slice_append(&waiting, running.items[current_ctx]);
    slice_append(&polls, poll);
    slice_remove(&running, current_ctx);
    runtime_continue();
}



void runtime_continue(void)
{
    // Switch to the next running coroutine
    if (polls.count > 0) {
        int timeout = running.count > 1 ? 0 : -1;
        int result = poll(polls.items, polls.count, timeout);
        assert(result >= 0 && "poll failed");

        // Wake up the first sleeping coroutine
        for (size_t i = 0; i < polls.count;) {
            // If the coroutine is sleeping, wake it up
            if (polls.items[i].revents) {
                size_t ctx = waiting.items[i];
                slice_remove(&polls, i);
                slice_remove(&waiting, i);
                slice_append(&running, ctx);
            } else { ++i; }
        }
    }

    // Ensure coroutines continue executing
    if (running.count > 0) {
        current_ctx %= running.count;
        runtime_restore(contexts.items[running.items[current_ctx]].stack_ptr);
    }
}


void runtime_finish_current(void)
{
    printf("runtime_finish_current\n");

    if (running.items[current_ctx] == 0) {
        fprintf(stderr, "[ERROR] Main coroutine with id 0 should never finish\n");
        exit(1);
    }

    slice_append(&stopped, running.items[current_ctx]);
    slice_remove(&running, current_ctx);

    if (polls.count > 0) {
        int timeout = running.count > 1 ? 0 : -1;
        int result = poll(polls.items, polls.count, timeout); // Wait for events
        assert(result >= 0);

        for (size_t i = 0; i < polls.count;) {
            if (polls.items[i].revents) {
                size_t ctx = waiting.items[i];
                slice_remove(&polls, i);
                slice_remove(&waiting, i);
                slice_append(&running, ctx);
            } else {
                ++i;
            }
        }
    }

    // Ensure we don't stop if there's at least one coroutine available
    if (running.count == 0 && waiting.count > 0) {
        slice_append(&running, waiting.items[0]);
        slice_remove(&waiting, 0);
        slice_remove(&polls, 0);
    }

    if (running.count > 0) {
        current_ctx %= running.count;
        runtime_restore(contexts.items[running.items[current_ctx]].stack_ptr);
    }
}


#else
// Making user experience better, this will be the first error you get if not undefined reference.
#define runtime_run(fn) \
    do { \
        printf("Implementation missing, please add #define RUNTIME_IMPLEMENTATION before you #import \"runtime.h\"\n"); \
        exit(1); \
    } while (0);
#endif // RUNTIME_IMPLEMENTATION
#endif // RUNTIME_HEADER