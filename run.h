
#ifndef RUN_HEADER
#define RUN_HEADER

#include <assert.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define array_append(arr, item) \
    do { \
        if ((arr)->count >= (arr)->capacity) { \
            (arr)->capacity = (arr)->capacity == 0 ? 256 : (arr)->capacity*2; \
            (arr)->items = realloc((arr)->items, sizeof(item) * (arr)->capacity); \
        } \
        (arr)->items[(arr)->count++] = (item); \
    } while (0);

#define array_remove(arr, i) \
    do { \
        size_t j = i; \
        assert(j < (arr)->count && "Index out of bounds"); \
        (arr)->items[i] = (arr)->items[--(arr)->count]; \
    } while (0);



// List used in runtime object
struct RuntimeList {
    size_t *items;
    size_t count;
    size_t capacity;
};

// Runtime object for async io
typedef struct {
    size_t current_ctx;

    struct RuntimeList active;
    struct RuntimeList asleep;
    struct RuntimeList inactive;

    struct RuntimeContexts {
        struct RuntimeContext {
            void *pointer;
            void *call_stack;
        } *items;

        size_t count;
        size_t capacity;
    } contexts;

    struct RuntimePolls {
        struct pollfd *items;
        size_t count;
        size_t capacity;
    } polls;
} Runtime;

void runtime_init(Runtime *r);

void runtime_run(Runtime *r, void (*fn)(void*), void *arg);

void runtime_yield(Runtime *r);

size_t runtime_id(Runtime *r);

size_t runtime_count(Runtime *r);

void runtime_sleep_read(Runtime *r, int fd);

void runtime_sleep_write(Runtime *r, int fd);

void runtime_wake_up(Runtime *r, size_t id);

void runtime_run_forever(Runtime *r);

void runtime_run_while_active(Runtime *r);

typedef struct ChannelHandler {
    enum {
        CHAN_SENDER,
        CHAN_RECEIVER,
    } type;

    int sequence_num;
    int reference_count;
    int processed_count;

    struct ClauseList {
        struct ChannelClause {
            struct ChannelHandler *handler;
            struct RuntimeContext *context;
            void *value;
            int index;
            bool available;
            bool used;
        } *items;

        size_t count;
        size_t capacity;
    } clauses;
} ChannelHandler;

typedef struct {
    // Size of the element type
    size_t size;

    ChannelHandler sender;
    ChannelHandler receiver;

    int reference_count;
    bool channel_done;

    size_t max_capacity;
    size_t item_count;
    size_t first_item;
} Channel;

Channel *channel(Runtime *r);

int channel_send(Channel *c, void *type, void *inst);

int channel_recv(Channel *c, void *type, void *inst);

void channel_close(Channel *c);

// Memory page that can be individually free
struct MemPage {
    struct MemPage *next;
    size_t count;
    size_t capacity;
    uintptr_t data[];
};

// Memory arena for memory management
typedef struct {
    struct MemPage *start, *end;
} MemArena;

struct MemPage *mem_page(size_t size);

void free_mem_page(struct MemPage* p);


#ifdef RUN_IMPLEMENTATION


void runtime_init(Runtime *r)
{
    r->current_ctx = 0;
    r->active = (struct RuntimeList){0};
    r->asleep = (struct RuntimeList){0};
    r->inactive = (struct RuntimeList){0};
    r->contexts = (struct RuntimeContexts){0};
    r->polls = (struct RuntimePolls){0};
    array_append(&r->contexts, (struct RuntimeContext){0});
    array_append(&r->active, 0);
}


void __attribute__((naked)) runtime_resume(Runtime *r, void *rsp)
{
    (void) r;(void)rsp; asm(
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


void runtime_finish_current(Runtime *r)
{
    if (r->active.items[r->current_ctx] == 0) {
        fprintf(stderr, "[ERROR] Main coroutine with id 0 should never finish\n");
        exit(1);
    }

    array_append(&r->inactive, r->active.items[r->current_ctx]);
    array_remove(&r->active, r->current_ctx);


    if (r->polls.count > 0) {
        int result = poll(r->polls.items, r->polls.count, -1);
        assert(result >= 0);
        for (size_t i = 0; i < r->polls.count;) {
            if (r->polls.items[i].revents) {
                size_t ctx = r->asleep.items[i];
                array_remove(&r->polls, i);
                array_remove(&r->asleep, i);
                array_append(&r->active, ctx);
            } else {
                ++i;
            }
        }
    }

    if (r->active.count == 0 && r->asleep.count > 0) {
        array_append(&r->active, r->asleep.items[0]);
        array_remove(&r->asleep, 0);
        array_remove(&r->polls, 0);
    }

    assert(r->active.count > 0);
    r->current_ctx %= r->active.count;
    runtime_resume(r, r->contexts.items[r->active.items[r->current_ctx]].pointer);
}

#define RUNTIME_STACK_SIZE 1024 * getpagesize()

void runtime_run(Runtime *r, void (*fn)(void*), void *arg)
{
    printf("running coroutine\n");
    size_t id;
    if (r->inactive.count > 0) {
        id = r->inactive.items[--r->inactive.count];
    } else {
        array_append(&r->contexts, (struct RuntimeContext){0});
        id = r->contexts.count - 1;
        r->contexts.items[id].call_stack = mmap(NULL, RUNTIME_STACK_SIZE, 
            PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK|MAP_GROWSDOWN, -1, 0);
        assert(r->contexts.items[id].call_stack != MAP_FAILED);
    }

    printf("creating coroutine %lu\n", id);
    void **pointer = (void**)((char*)r->contexts.items[id].call_stack + RUNTIME_STACK_SIZE);
    
    *(--pointer) = runtime_finish_current;  // Return address
    *(--pointer) = (void *) fn;             // Function to run
    *(--pointer) = arg;                     // Function argument
    *(--pointer) = 0;                        // RBP
    *(--pointer) = 0;                        // RBX
    *(--pointer) = 0;                        // R12
    *(--pointer) = 0;                        // R13
    *(--pointer) = 0;                        // R14
    *(--pointer) = 0;                        // R15
    *(--pointer) = 0;                        // Alignment

    r->contexts.items[id].pointer = pointer;

    printf("appending coroutine %lu\n", id);
    array_append(&r->active, id);
}


typedef enum {
    SM_NONE = 0,
    SM_READ,
    SM_WRITE,
} SleepMode;

void runtime_switch_context(Runtime *r, void *rsp, SleepMode sm, int fd)
{
    r->contexts.items[r->active.items[r->current_ctx]].pointer = rsp;

    switch (sm) {
        case SM_NONE:
            r->current_ctx = (r->current_ctx + 1) % r->active.count; // Rotate
            break;

        case SM_READ: {
            array_append(&r->asleep, r->active.items[r->current_ctx]);
            struct pollfd poll = {.fd = fd, .events = POLLIN};
            array_append(&r->polls, poll);
            array_remove(&r->active, r->current_ctx);
        } break;

        case SM_WRITE: {            
            array_append(&r->asleep, r->active.items[r->current_ctx]);
            struct pollfd poll = {.fd = fd, .events = POLLOUT};
            array_append(&r->polls, poll);
            array_remove(&r->active, r->current_ctx);
        } break;

        default:
            fprintf(stderr, "[ERROR] Unknown sleep mode\n");
            exit(1);
    }

    // Non-blocking poll check
    if (r->polls.count > 0) {
        int result = poll(r->polls.items, r->polls.count, 0); // Use 0 to avoid blocking
        assert(result >= 0 && "poll failed");

        for (size_t i = 0; i < r->polls.count;) {
            if (r->polls.items[i].revents) {
                size_t ctx = r->asleep.items[i];
                array_remove(&r->polls, i);
                array_remove(&r->asleep, i);
                array_append(&r->active, ctx);
            } else {
                ++i;
            }
        }
    }

    // Ensure coroutines continue executing
    if (r->active.count > 0) {
        r->current_ctx %= r->active.count;
        runtime_resume(r, r->contexts.items[r->active.items[r->current_ctx]].pointer);
    }
}

void __attribute__((naked)) runtime_yield(Runtime *r)
{
    (void)r; asm volatile (
    "    pushq %rdi\n"
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rdi, %rdi\n"
    "    movq %rsp, %rsi\n"
    "    movq $0, %rdx\n"
    "    jmp runtime_switch_context\n"
    );
}


void runtime_sleep_read(Runtime *r, int fd)
{
    (void)fd; asm(
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rdx, %rcx\n"     // Save fd in %rcx
    "    movq %rsp, %rsi\n"     // Save stack pointer in %rsi
    "    movq $1, %rdx\n"       // SleepMode = SM_READ
    "    popq %rdi\n"           // Restore r into %rdi
    "    jmp runtime_switch_context\n");
}

void runtime_sleep_write(Runtime *r, int fd)
{
    (void)r; (void)fd; asm(
    "    pushq %rbp\n"
    "    pushq %rbx\n"
    "    pushq %r12\n"
    "    pushq %r13\n"
    "    pushq %r14\n"
    "    pushq %r15\n"
    "    movq %rdx, %rcx\n"     // Save fd in %rcx
    "    movq %rsp, %rsi\n"     // Save stack pointer in %rsi
    "    movq $2, %rdx\n"       // SleepMode = SM_WRITE
    "    popq %rdi\n"           // Restore r into %rdi
    "    jmp runtime_switch_context\n");
}
    
void runtime_wake_up(Runtime *r, size_t id)
{
    for (size_t i = 0; i < r->asleep.count; ++i) {
        if (r->asleep.items[i] == id) {
            array_remove(&r->asleep, id);
            array_remove(&r->polls, id);
            array_append(&r->active, id);
            return;
        }
    }
}

void runtime_run_forever(Runtime *r)
{
    while (true) runtime_yield(r);
}

void runtime_run_while_active(Runtime *r)
{
    while (runtime_count(r) > 1) runtime_yield(r);
}

size_t runtime_id(Runtime *r)
{
    return r->active.items[r->current_ctx];
}

size_t runtime_count(Runtime *r)
{
    return r->active.count;
}


#endif // RUN_IMPLEMENTATION
#endif // RUN_HEADER