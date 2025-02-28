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


// Init a new coroutine 
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

// Run the runtime forever allowing for continuous execution.
void runtime_run_forever(void);

// Run the runtime until there are no more alive coroutines.
void runtime_run_while_active(void);


////////////////////
// Networking API //
////////////////////

#include <sys/socket.h>

typedef struct {
    char *host;
    int port;
    void (*fn)(void*);
} TcpServer;

typedef struct {
    TcpServer *server;
    int client_fd;
} TcpConn;

void tcp_listen(char *addr, int port, void (*fn)(void *conn));
int tcp_read(int fd, char *buf, int size);
int tcp_read_until(int fd, char *buf, int size, char *delim);
int tcp_write(int fd, char *buf, int size);


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
            (arr)->cap = (arr)->cap == 0 ? 256 : (arr)->cap * 2; \
            (arr)->items = realloc((arr)->items, sizeof(item) * (arr)->cap); \
        } \
        (arr)->items[(arr)->count++] = (item); \
    } while (0);

#define array_remove(arr, i) \
    do { \
        size_t j = i; \
        assert(j < (arr)->count && "Index out of bounds"); \
        (arr)->items[i] = (arr)->items[--(arr)->count]; \
    } while (0);


////////////////////////
// Async Runtime Impl //
////////////////////////

static size_t current_ctx       = 0;
static RuntimeFrame active      = {0};
static RuntimeFrame asleep      = {0};
static RuntimeFrame inactive    = {0};
static RuntimeContexts contexts = {0};
static RuntimePolls polls       = {0};

void runtime_init(void)
{
    printf("initializing runtime\n");
    if (contexts.count > 0) return;
    array_append(&contexts, (RuntimeContext){0});
    array_append(&active, 0);
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
    if (active.items[current_ctx] == 0)
        NOB_UNREACHABLE("Main coroutine with id 0 should never finish");

    printf("finishing coroutine %lu\n", active.items[current_ctx]);
    array_append(&inactive, active.items[current_ctx]);
    array_remove(&active, current_ctx);

    printf("polling for events\n");
    if (polls.count > 0) {
        int result = poll(polls.items, polls.count, -1); // Wait for events
        assert(result >= 0);
        for (size_t i = 0; i < polls.count;) {
            if (polls.items[i].revents) {
                size_t ctx = asleep.items[i];
                array_remove(&polls, i);
                array_remove(&asleep, i);
                array_append(&active, ctx);
            } else {
                ++i;
            }
        }
    }

    // Ensure we don't stop if there's at least one coroutine available
    if (active.count == 0 && asleep.count > 0) {
        printf("Waking up the first sleeping coroutine\n");
        array_append(&active, asleep.items[0]);
        array_remove(&asleep, 0);
        array_remove(&polls, 0);
    }

    assert(active.count > 0);
    current_ctx %= active.count;
    runtime_resume(contexts.items[active.items[current_ctx]].pointer);
}

#define STACK_SIZE 1024 * getpagesize()


void runtime_run(void (*fn)(void*), void *arg)
{
    printf("running coroutine\n");
    size_t id;
    if (inactive.count > 0) {
        id = inactive.items[--inactive.count];
    } else {
        array_append(&contexts, ((RuntimeContext){0}));
        id = contexts.count - 1;
        contexts.items[id].stack = mmap(NULL, STACK_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK|MAP_GROWSDOWN, -1, 0);
        assert(contexts.items[id].stack != MAP_FAILED);
    }

    printf("creating coroutine %lu\n", id);
    void **pointer = (void**)((char*)contexts.items[id].stack + STACK_SIZE - sizeof(void*));
    *(--pointer) = runtime_finish_current;
    *(--pointer) = fn;
    *(--pointer) = arg;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    contexts.items[id].pointer = pointer;

    printf("appending coroutine %lu\n", id);
    array_append(&active, id);
}


size_t runtime_id(void)
{
    return active.items[current_ctx];
}


size_t runtime_count(void)
{
    return active.count;
}


void runtime_wake_up(size_t id)
{
    printf("waking up coroutine %lu\n", id);
    for (size_t i = 0; i < asleep.count; ++i) {
        if (asleep.items[i] == id) {
            array_remove(&asleep, id);
            array_remove(&polls, id);
            array_append(&active, id);
            return;
        }
    }
}


typedef enum {
    SM_NONE = 0,
    SM_READ,
    SM_WRITE,
} SleepMode;

void runtime_switch_context(void *rsp, SleepMode sm, int fd) {
    contexts.items[active.items[current_ctx]].pointer = rsp;

    switch (sm) {
        case SM_NONE:
            current_ctx = (current_ctx + 1) % active.count; // Rotate
            break;

        case SM_READ: {
            array_append(&asleep, active.items[current_ctx]);
            struct pollfd poll = {.fd = fd, .events = POLLIN};
            array_append(&polls, poll);
            array_remove(&active, current_ctx);
        } break;

        case SM_WRITE: {
            array_append(&asleep, active.items[current_ctx]);
            struct pollfd poll = {.fd = fd, .events = POLLOUT};
            array_append(&polls, poll);
            array_remove(&active, current_ctx);
        } break;

        default:
            NOB_UNREACHABLE("Unknown sleep mode");
    }

    // Non-blocking poll check
    if (polls.count > 0) {
        int result = poll(polls.items, polls.count, 0); // Use 0 to avoid blocking
        assert(result >= 0 && "poll failed");

        for (size_t i = 0; i < polls.count;) {
            if (polls.items[i].revents) {
                size_t ctx = asleep.items[i];
                array_remove(&polls, i);
                array_remove(&asleep, i);
                array_append(&active, ctx);
            } else {
                ++i;
            }
        }
    }

    // Ensure coroutines continue executing
    if (active.count > 0) {
        current_ctx %= active.count;
        runtime_resume(contexts.items[active.items[current_ctx]].pointer);
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

void runtime_run_forever(void)
{
    while (true) runtime_yield();
}

void runtime_run_while_active(void)
{
    while (runtime_count() > 1) runtime_yield();
}


/////////////////////
// Networking Impl //
/////////////////////


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


int tcp_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return flags;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void tcp_handle_conn(void *arg)
{
    TcpConn *conn = (TcpConn*)arg;
    conn->server->fn(conn);
    runtime_yield();
    close(conn->client_fd);
    free(conn);
}

void _tcp_listen(void *arg)
{
    TcpServer *server = (TcpServer*)arg;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0 && "Failed to create socket");

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(server->host),
        .sin_port = htons(server->port),
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Failed to bind to socket");
        close(fd);
        return;
    }

    if (listen(fd, 128) < 0) {
        perror("Failed to listen on socket");
        close(fd);
        return;
    }

    if (tcp_nonblock(fd) < 0) {
        perror("Failed to set socket non-blocking");
        close(fd);
        return;
    }

    while (1) {
        runtime_sleep_read(fd); // Sleep until a new connection is ready

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int conn_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
        if (conn_fd < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                continue; // No connection yet, sleep again
            }
            perror("accept failed");
            continue;
        }

        if (tcp_nonblock(conn_fd) < 0) {
            perror("Failed to set connection to non-blocking");
            close(conn_fd);
            continue;
        }

        TcpConn *conn = malloc(sizeof(TcpConn));
        conn->server = server;
        conn->client_fd = conn_fd;
        runtime_run(tcp_handle_conn, conn);
    }
}


void tcp_listen(char *host, int port, void (*fn)(void*))
{
    TcpServer *server = malloc(sizeof(TcpServer));
    server->host = strdup(host);
    server->port = port;
    server->fn = fn;
    runtime_run(_tcp_listen, server);
}

int tcp_read(int fd, char *buf, int size)
{
    return tcp_read_until(fd, buf, size, "\0");
}

int tcp_read_until(int fd, char *buf, int size, char *delim)
{
    int total_read = 0;
    while (total_read < size - 1) { // Ensure room for null termination
        int n = read(fd, buf + total_read, size - 1 - total_read);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_sleep_read(fd);
                continue;
            }
            perror("tcp_read failed");
            return -1;
        } else if (n == 0) {
            return -1; // Connection closed
        }
        total_read += n;

        // Check if we've received the end of an HTTP header (`\r\n\r\n`)
        if (strstr(buf, delim)) {
            break;
        }
    }
    buf[total_read] = '\0'; // Ensure null-terminated string
    return total_read;
}

int tcp_write(int fd, char *buf, int size)
{
    int total_written = 0;
    while (total_written < size) {
        int n = write(fd, buf + total_written, size - total_written);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                runtime_sleep_write(fd);
                continue;
            }
            perror("tcp_write failed");
            return -1;
        }
        total_written += n;
    }
    return total_written;
}


#endif // APP_IMPLEMENTATION
#endif // APP_HEADER