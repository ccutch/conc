#ifndef PROTO_HEADER
#define PROTO_HEADER

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
#define slice_append(slice, item) \
    do { \
        if ((slice)->count >= (slice)->capacity) { \
            (slice)->capacity = (slice)->capacity == 0 ? 256 : (slice)->capacity*2; \
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

typedef enum {
    RUNTIME_MODE_NONE  = 0,
    RUNTIME_MODE_READ  = 1,
    RUNTIME_MODE_WRITE = 2,
} RuntimeMode;

typedef struct {
    void *stack_ptr;
    void *call_stack;
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

void runtime_init(void);
void runtime_main(void);
size_t runtime_id(void);
void runtime_yield(void);
void runtime_restore(void *rsp);
void runtime_continue(void *rsp, RuntimeMode mode, int fd);
void runtime_finish_current(void);



#ifdef PROTO_IMPLEMENTATION

static size_t current_ctx           = 0;
static NumSlice active              = {0};
static NumSlice asleep              = {0};
static NumSlice inactive            = {0};
static RuntimeContextSlice contexts = {0};
static RuntimePollSlice polls       = {0};

void runtime_finish_current(void)
{
    if (active.items[current_ctx] == 0) {
        fprintf(stderr, "[ERROR] Main coroutine with id 0 should never finish\n");
        exit(1);
    }

    printf("finishing coroutine %lu\n", active.items[current_ctx]);
    slice_append(&inactive, active.items[current_ctx]);
    slice_remove(&active, current_ctx);

    printf("polling for events\n");
    if (polls.count > 0) {
        int result = poll(polls.items, polls.count, -1); // Wait for events
        assert(result >= 0);
        for (size_t i = 0; i < polls.count;) {
            if (polls.items[i].revents) {
                size_t ctx = asleep.items[i];
                slice_remove(&polls, i);
                slice_remove(&asleep, i);
                slice_append(&active, ctx);
            } else {
                ++i;
            }
        }
    }

    // Ensure we don't stop if there's at least one coroutine available
    if (active.count == 0 && asleep.count > 0) {
        printf("Waking up the first sleeping coroutine\n");
        slice_append(&active, asleep.items[0]);
        slice_remove(&asleep, 0);
        slice_remove(&polls, 0);
    }

    assert(active.count > 0);
    current_ctx %= active.count;
    runtime_restore(contexts.items[active.items[current_ctx]].stack_ptr);
}

#define STACK_SIZE 1024 * 4

void runtime_start(void (*fn)(void*))
{
    printf("running coroutine\n");
    size_t id;
    if (inactive.count > 0) {
        id = inactive.items[--inactive.count];
    } else {
        slice_append(&contexts, ((RuntimeContext){0}));
        id = contexts.count - 1;
        contexts.items[id].call_stack = mmap(NULL, STACK_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK|MAP_GROWSDOWN, -1, 0);
        assert(contexts.items[id].call_stack != MAP_FAILED);
    }

    printf("creating coroutine %lu\n", id);
    void **pointer = (void**)((char*)contexts.items[id].call_stack + STACK_SIZE - sizeof(void*));
    *(--pointer) = runtime_finish_current;
    *(--pointer) = fn;
    *(--pointer) = NULL;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    contexts.items[id].stack_ptr = pointer;

    printf("appending coroutine %lu\n", id);
    slice_append(&active, id);
}


#define runtime_run(fn) \
    do { \
      void __fn__(void) { fn; } \
      runtime_start((void*)__fn__); \
    } while (0);


void runtime_init(void)
{
    if (contexts.count > 0) return;
    slice_append(&contexts, (RuntimeContext){0});
    slice_append(&active, 0);
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

void runtime_continue(void *rsp, RuntimeMode mode, int fd)
{
    contexts.items[active.items[current_ctx]].stack_ptr = rsp;

    switch (mode) {
        case RUNTIME_MODE_NONE:
            current_ctx = (current_ctx + 1) % active.count; // Rotate
            break;

        case RUNTIME_MODE_READ: {
            slice_append(&asleep, active.items[current_ctx]);
            struct pollfd poll = {.fd = fd, .events = POLLIN};
            slice_append(&polls, poll);
            slice_remove(&active, current_ctx);
        } break;

        case RUNTIME_MODE_WRITE: {
            slice_append(&asleep, active.items[current_ctx]);
            struct pollfd poll = {.fd = fd, .events = POLLOUT};
            slice_append(&polls, poll);
            slice_remove(&active, current_ctx);
        } break;

        default:
            fprintf(stderr, "[ERROR] Unknown sleep mode\n");
            exit(1);
    }

    // Non-blocking poll check
    if (polls.count > 0) {
        int result = poll(polls.items, polls.count, 0); // Use 0 to avoid blocking
        assert(result >= 0 && "poll failed");

        for (size_t i = 0; i < polls.count;) {
            if (polls.items[i].revents) {
                size_t ctx = asleep.items[i];
                slice_remove(&polls, i);
                slice_remove(&asleep, i);
                slice_append(&active, ctx);
            } else {
                ++i;
            }
        }
    }

    // Ensure coroutines continue executing
    if (active.count > 0) {
        current_ctx %= active.count;
        runtime_restore(contexts.items[active.items[current_ctx]].stack_ptr);
    }
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
        "    jmp runtime_continue\n");
}

size_t runtime_id(void)
{
    return active.items[current_ctx];
}

void runtime_main(void)
{
    while (active.count > 1) runtime_yield();
}


#endif // PROTO_IMPLEMENTATION
#endif // PROTO_HEADER