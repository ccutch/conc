#ifndef APP_HEADER
#define APP_HEADER

#include <poll.h>
#include <stddef.h>

///////////////////////
// Async Runtime API //
///////////////////////

typedef struct {
    void *pointer;
    void *stack;
} RuntimeContext;

typedef struct {
    RuntimeContext *items;
    size_t count;
    size_t cap;
} RuntimeContexts;

typedef struct {
    size_t *items;
    size_t count;
    size_t cap;
} RuntimeFrame;

typedef struct {
    struct pollfd *items;
    size_t count;
    size_t cap;
} RuntimePolls;

// Coroutine based runtime for handling http and file io.
typedef struct {
    size_t current_ctx;
    RuntimeContexts contexts;
    RuntimeFrame active;
    RuntimeFrame asleep;
    RuntimeFrame inactive;
    RuntimePolls polls;
} Runtime;

// Init a new coroutine runtime.
void runtime_init(void);

// Yield resources to the next coroutine.
void runtime_yield(void);

// Create a new coroutine.
void runtime_run(void (*fn)(void*), void *arg);

// Return the id of the current coroutine.
size_t runtime_id(void);

// Return the count of current coroutines.
size_t runtime_count(void);

// Sleep current coroutine until the non-blocking file descriptor
// has available data to read.
void runtime_sleep_read(int);

// Sleep current coroutine until the non-blocking file descriptor
// is available to write data.
void runtime_sleep_write(int);

// Wake up a coroutine by id if it is sleeping do to a sleep.
void runtime_wake_up(size_t);


#ifdef APP_IMPLEMENTATION

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define NOB_IMPLEMENTATION
#include "nob.h"

/////////////////
// Collections //
/////////////////

#define array_append(arr, item) \
    do { \
        if ((arr)->count >= (arr)->cap) { \
            (arr)->cap = (arr)->cap == 0 ? (arr)->cap * 2 : 256; \
            (arr)->items = realloc((arr)->items, sizeof(item) * (arr)->cap); \
        } \
        (arr)->items[(arr)->count++] = (item); \
    } while (0)

#define array_remove(arr, i) \
    do { \
        size_t j = i; \
        assert(j < (arr)->count && "Index out of bounds"); \
        (arr)->items[i] = (arr)->items[--(arr)->count]; \
    } while (0)

////////////////////////
// Async Runtime Impl //
////////////////////////

static Runtime runtime = {0};

void runtime_init(void)
{
    nob_log(NOB_INFO, "initializing runtime");
    if (runtime.contexts.count > 0) return;
    nob_log(NOB_INFO, "initializing runtime contexts");
    array_append(&runtime.contexts, (RuntimeContext){0});
    array_append(&runtime.active, 0);
}


void __attribute((naked)) runtime_resume(void *rsp)
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


void runtime_finish_current(void)
{
    if (runtime.active.items[runtime.current_ctx] == 0)
        NOB_UNREACHABLE("Main coroutine with id 0 should never finish");

    nob_log(NOB_INFO, "finishing coroutine %lu", runtime.active.items[runtime.current_ctx]);
    array_append(&runtime.inactive, runtime.active.items[runtime.current_ctx]);
    array_remove(&runtime.active, runtime.current_ctx);

    nob_log(NOB_INFO, "polling for events");
    if (runtime.polls.count > 0) {
        int result = poll(runtime.polls.items, runtime.polls.count, 0);
        assert(result >= 0);
        for (size_t i = 0; i < runtime.polls.count;) {
            if (runtime.polls.items[i].revents) {
                size_t ctx = runtime.asleep.items[i];
                array_remove(&runtime.polls, i);
                array_remove(&runtime.asleep, i);
                array_append(&runtime.active, ctx);
            } else {
                ++i;
            }
        }
    }

    nob_log(NOB_INFO, "resuming coroutine %lu", runtime.active.items[runtime.current_ctx]);
    assert(runtime.active.count > 0);
    runtime.current_ctx %= runtime.active.count;
    runtime_resume(runtime.contexts.items[runtime.active.items[runtime.current_ctx]].pointer);
}

#define STACK_SIZE 1024 * getpagesize()

void runtime_run(void (*fn)(void*), void *arg)
{
    nob_log(NOB_INFO, "running coroutine");
    size_t id;
    if (runtime.inactive.count > 0) {
        id = runtime.inactive.items[--runtime.inactive.count];
    } else {
        array_append(&runtime.contexts, (RuntimeContext){0});
        id = runtime.contexts.count - 1;
        runtime.contexts.items[id].stack = mmap(NULL, STACK_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK|MAP_GROWSDOWN, -1, 0);
        assert(runtime.contexts.items[id].stack != MAP_FAILED);
    }

    nob_log(NOB_INFO, "creating coroutine %lu", id);
    void **pointer = (void**)((char*)runtime.contexts.items[id].stack + STACK_SIZE - sizeof(void*));
    *(--pointer) = runtime_finish_current;
    *(--pointer) = fn;
    *(--pointer) = arg;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    runtime.contexts.items[id].pointer = pointer;

    nob_log(NOB_INFO, "appending coroutine %lu to active list", id);
    array_append(&runtime.active, id);
}

size_t runtime_id(void)
{
    return runtime.active.items[runtime.current_ctx];
}

size_t runtime_count(void)
{
    return runtime.active.count;
}

void runtime_wake_up(size_t id)
{
    nob_log(NOB_INFO, "waking up coroutine %lu", id);
    for (size_t i = 0; i < runtime.asleep.count; ++i) {
        if (runtime.asleep.items[i] == id) {
            array_remove(&runtime.asleep, id);
            array_remove(&runtime.polls, id);
            array_append(&runtime.active, id);
            return;
        }
    }
}

typedef enum {
    SM_NONE = 0,
    SM_READ,
    SM_WRITE,
} SleepMode;


void runtime_switch_context(void *rsp, SleepMode sm, int fd)
{
    runtime.contexts.items[runtime.active.items[runtime.current_ctx]].pointer = rsp;

    switch (sm) {
        case SM_NONE: runtime.current_ctx += 1; break;

        case SM_READ: {
            array_append(&runtime.asleep, runtime.active.items[runtime.current_ctx]);
            struct pollfd poll = {.fd = fd, .events = POLLRDNORM};
            array_append(&runtime.polls, poll);
            array_remove(&runtime.active, runtime.current_ctx);
        } break;

        case SM_WRITE: {
            array_append(&runtime.asleep, runtime.active.items[runtime.current_ctx]);
            struct pollfd poll = {.fd = fd, .events = POLLWRNORM};
            array_append(&runtime.polls, poll);
            array_remove(&runtime.active, runtime.current_ctx);
        } break;
        
        default:
            NOB_UNREACHABLE("Unknown sleep mode");
    }

    if (runtime.polls.count > 0) {
        int result = poll(runtime.polls.items, runtime.polls.count, 0);
        assert(result >= 0 && "poll failed");

        for (size_t i = 0; i < runtime.polls.count;) {
            if (runtime.polls.items[i].revents) {
                size_t ctx = runtime.asleep.items[i];
                array_remove(&runtime.polls, i);
                array_remove(&runtime.asleep, i);
                array_append(&runtime.active, ctx);
            } else {
                ++i;
            }
        }
    }

    assert(runtime.active.count > 0 && "No active coroutines");
    runtime.current_ctx %= runtime.active.count;
    runtime_resume(runtime.contexts.items[runtime.active.items[runtime.current_ctx]].pointer);
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
    "    movq $0, %rsi\n"
    "    jmp runtime_switch_context\n");
}

void __attribute__((naked)) runtime_sleep_read(int fd)
{
    (void) fd; asm(
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rdi, %rdx\n"
    "    movq %rsp, %rdi\n"
    "    movq $1, %rsi\n"
    "    jmp runtime_switch_context\n");
}

void __attribute__((naked)) runtime_sleep_write(int fd)
{
    (void) fd; asm(
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rdi, %rdx\n"     // fd
    "    movq %rsp, %rdi\n"     // rsp
    "    movq $2, %rsi\n"       // sm = SM_WRITE
    "    jmp runtime_switch_context\n");
}



#endif // APP_IMPLEMENTATION
#endif // APP_HEADER