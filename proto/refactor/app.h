/** app.h - A framework for building high performance web apps.

    Abstract
    ========
    This library provides a foundation for our application server.
    It will manage memory, schedule processes, handle non-blocking
    file I/O, parse HTTP requests, and more. Also I'm like bored.
    
    Table of Contents:
    ===================
    Structures - provides common data structures we need to build
    on top of when we have more complicated states. We are mostly
    using lists (aka dynamic arrays) to store most of our data.

    Runtime - provides a foundation for our application server.
    It will manage memory via memory pages that can be associated
    to a process. These processes will be cooperatively scheduled
    and will clean up memory when they are done.

    Network - provides a non-blocking tcp server that will handle
    incoming connections and manage the state of the connections.
    We also provide a series of functions to handle non-blocking
    I/O on the sockets. 
    
    Data - provides a system for labeling and storing data in a
    way that leverages our runtime memory system, integrates well
    with json, and provides features like reflection that are key
    to the HTML templating engine we will add later.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-01
    @version  0.1.0
*/


#ifndef APP_HEADER
#define APP_HEADER


#include <stdbool.h>


#define RUNTIME_LIST_SIZE 512 
#define RUNTIME_PROC_SIZE 8 * getpagesize()
#define RUNTIME_PAGE_SIZE 1 * getpagesize()


////////////////
// STRUCTURES //
////////////////

// We need a dynamic way to store data about the current state of
// our application and the processes that are running. Providing
// a few common containers for storing data, using the name list.

// Generic list of integer values, primarily used for storing
// the ids of processes in their different states
struct IntSlice {
    int *items;
    int capacity;
    int count;
};

// Generic list of strings, primarily used for storing lists of
// null terminated strings
struct StrSlice {
    char **items;
    int capacity;
    int count;
};

// appeds a new item to the list's memory after checking if the
// capacity is full. This will work generically for all lists.
#define slice_append(list, item) ({ \
    if ((list)->count >= (list)->capacity) { \
        (list)->capacity += RUNTIME_LIST_SIZE; \
        (list)->items = realloc((list)->items, (list)->capacity * sizeof(item)); \
    } \
    (list)->items[(list)->count++] = (item); \
})

// removes an item from the list's memory and replaces it with
// the last item in the list, while also decrementing the count
#define slice_remove(list, index) ({ \
    if ((index) >= (list)->count) { \
        fprintf(stderr, "[ERROR] Index out of bounds\n"); \
        exit(1); \
    } \
    (list)->items[index] = (list)->items[--(list)->count]; \
})


/////////////
// RUNTIME //
/////////////

// RuntimePage is a region of memory that will be dynamically allocated
// and deallocated all at once, when the process which created it is done
struct RuntimePage {
    
    // When a page is full capacity we will allocate a new page
    // and store a reference to 
    struct RuntimePage *next;

    // The size of the page, named count to match other structs
    int count;

    // The capacity of the page, given when page is created
    int capacity;

    // A pointer to the memory that the page is using
    char *memory;

};

// RuntimePage constructor and destructor functions called from Proc
struct RuntimePage *runtime_new_page(int capacity);
void runtime_destroy_page(struct RuntimePage *page);


// RuntimeProc is a cooperative process that will be scheduled
// and will clean up memory when it is done. This provides a
// backbone to our application server
struct RuntimeProc {

    // The stack pointer of the coroutine process
    void *stack_pointer;

    // A snapshot of the registers when the process
    // yields to another process
    void *registers;

    // Pointers to the memory pages with references
    // to the front and back of the linked list of
    // memory pages
    struct RuntimePage *head;
    struct RuntimePage *tail;

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
    void __thread__(void) { fn; } \
    runtime_start((void*)__thread__, NULL); \
})
#else
// This feature is only available for gcc due to the anonymous fn
#define runtime_run(fn) ({ \
    fprintf(stderr, "[ERROR] runtime_run is only available for gcc\n"); \
    exit(1); \
})
#endif

// Macro to print the total size of all pages
#define runtime_total_page_size(page) ({ \
    int size = 0; \
    while (page != NULL) { \
        size += page->capacity; \
        page = page->next; \
    } \
    printf("total page size: %d\n", size); \
})


/////////////
// Network //
/////////////


struct NetworkRequest {
    int conn_fd;
    struct StrSlice req_headers;
    struct StrSlice res_headers;
    char *method;
    char *path;
};

void network_listen(int port, void (*handler)(int fd));
int network_write(int fd, char *buf, int len);
int network_read(int fd, char *buf, int len);
int network_read_until(int fd, char *buf, int len, char delim);

struct NetworkRequest *network_parse_http(int fd);
void network_get_header(struct NetworkRequest *req, char *header, char *value);
void network_set_header(struct NetworkRequest *req, char *header, char *value);
void network_write_head(struct NetworkRequest *req, int status, char *message);
void network_write_body(struct NetworkRequest *req, char *body, int len);


//////////
// Data //
//////////


struct DataValue {
    enum {
        DATA_UNIT,
        DATA_TUPLE,
        DATA_LIST,
        DATA_MAP,
        DATA_INTEGER,
        DATA_NUMBER,
        DATA_STRING,
        DATA_BOOLEAN,
    } type;

    union {
        struct DataTuple {
            struct DataValue *left;
            struct DataValue *right;
        } tuple;

        struct DataList {
            struct DataValue **items;
            int count;
            int capacity;
        } list;

        struct DataMap {
            struct DataTuple **items;
            int count;
            int capacity;
        } map;

        int integer;
        char *string;
        bool boolean;
        double number;
    };
};

char *data_sprint(struct DataValue *value);
char *data_to_json(struct DataValue *value);

struct DataValue *data_parse_json(char *json);
struct DataValue *data_unit(void);
struct DataValue *data_tuple(struct DataValue *key, struct DataValue *val);
struct DataValue *data_list(struct DataValue *head, ...);
struct DataValue *data_map(struct DataValue *head, ...);
struct DataValue *data_integer(int value);
struct DataValue *data_number(double value);
struct DataValue *data_string(char *value);
struct DataValue *data_boolean(bool value);

#define DATA_END NULL


#ifdef APP_IMPLEMENTATION 


#include <poll.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>


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


struct RuntimePage *runtime_new_page(int capacity)
{
    // Allocating memory for the page and failing gracefully
    struct RuntimePage *page = malloc(sizeof(struct RuntimePage));
    if (page == NULL) return NULL;

    page->next = NULL;
    page->memory = malloc(capacity);
    if (page->memory == NULL) {
        free(page);
        return NULL;
    }

    page->capacity = capacity;
    page->count = 0;
    return page;
}


void runtime_destroy_page(struct RuntimePage *page)
{ free(page); }


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
        slice_append(&runtime_all_procs, ((struct RuntimeProc){0}));
        id = runtime_all_procs.count - 1;
        runtime_all_procs.items[id].head = runtime_new_page(RUNTIME_PAGE_SIZE);
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
            fprintf(stderr, "[ERROR] poll failed\n");
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
        fprintf(stderr, "[ERROR] Main coroutine with id 0 should never finish\n");
        exit(1);
    }

    struct RuntimeProc running_proc = runtime_all_procs.items[running_id];
    struct RuntimePage *page = running_proc.head;
    runtime_total_page_size(page);

    // Cleaning up memory pages
    page = running_proc.head;
    while ((page = running_proc.head) != NULL) {
        running_proc.head = page->next;
        runtime_destroy_page(page);
    }

    page = running_proc.head;
    runtime_total_page_size(page);

    slice_append(&runtime_stopped_procs, running_id);
    slice_remove(&runtime_running_procs, runtime_current_proc);

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
    int process_id = runtime_running_procs.items[runtime_current_proc];
    struct RuntimeProc *running_proc = &runtime_all_procs.items[process_id];

    if (running_proc->tail == NULL) {
        int page_size = RUNTIME_PAGE_SIZE > size ? RUNTIME_PAGE_SIZE : size;
        running_proc->tail = runtime_new_page(page_size);
        if (running_proc->tail == NULL) {
            fprintf(stderr, "[ERROR] Failed to allocate memory for runtime process\n");
            exit(1);
        }
        running_proc->head = running_proc->tail;
    }

    struct RuntimePage *page = running_proc->head;
    while (page->count + size > page->capacity && page->next != NULL) {
        page = page->next;
    }

    if (page->count + size > page->capacity) {
        int page_size = RUNTIME_PAGE_SIZE > size ? RUNTIME_PAGE_SIZE : size;
        page->next = runtime_new_page(page_size);
        running_proc->tail = page->next;
        page = page->next;
    }

    void *memory = &page->memory[page->count];
    page->count += size;
    return memory;
}

char *data_sprint(struct DataValue *value)
{
    switch (value->type) {
        case DATA_UNIT:
            return "Empty";

        case DATA_TUPLE: {
            char *right = data_sprint(value->tuple.right);
            char *left = data_sprint(value->tuple.left);
            int len = snprintf(NULL, 0, "(%s, %s)", left, right) + 1;
            char *res = runtime_alloc(len);
            snprintf(res, len, "(%s, %s)", left, right);
            return res;
        }

        case DATA_LIST: {
            if (value->list.count == 0) return "[]";
            int total_len = 2;  // For '[' and ']'
            char **items = malloc(value->list.count * sizeof(char *));
            
            for (int i = 0; i < value->list.count; i++) {
                items[i] = data_sprint(value->list.items[i]);
                total_len += strlen(items[i]) + 2;  // Extra for ", "
            }

            char *res = runtime_alloc(total_len);
            res[0] = '[';
            res[1] = '\0';

            for (int i = 0; i < value->list.count; i++) {
                strcat(res, items[i]);
                if (i < value->list.count - 1) strcat(res, ", ");
            }

            strcat(res, "]");
            free(items);
            return res;
        }

        case DATA_MAP: {
            if (value->map.count == 0) return "{}";
            int total_len = 2;
            char **items = malloc(value->map.count * sizeof(char *));
            
            for (int i = 0; i < value->map.count; i++) {
                struct DataValue *item = value->map.items[i];
                char *item_str = data_sprint(item);
                int len = snprintf(NULL, 0, "\"%s\": %s", value->map.keys[i], item_str) + 1;
                items[i] = runtime_alloc(len);
                snprintf(items[i], len, "\"%s\": %s", value->map.keys[i], item_str);
                total_len += len + 2;
            }

            char *res = runtime_alloc(total_len);
            res[0] = '{';
            res[1] = '\0';

            for (int i = 0; i < value->map.count; i++) {
                strcat(res, items[i]);
                if (i < value->map.count - 1) strcat(res, ", ");
            }

            strcat(res, "}");
            free(items);
            return res;
        }

        case DATA_INTEGER: {
            char *res = runtime_alloc(32);
            snprintf(res, 32, "%d", value->integer);
            return res;
        }

        case DATA_NUMBER: {
            char *res = runtime_alloc(32);
            snprintf(res, 32, "%f", value->number);
            return res;
        }

        case DATA_STRING: {
            int len = snprintf(NULL, 0, "\"%s\"", value->string) + 1;
            char *res = runtime_alloc(len);
            snprintf(res, len, "\"%s\"", value->string);
            return res;
        }

        case DATA_BOOLEAN:
            return value->boolean ? "true" : "false";

        default:
            return "invalid";
    }
}


char *data_to_json(struct DataValue *value)
{
    switch (value->type) {
        case DATA_UNIT: {
            return "null";
        }

        case DATA_TUPLE: {
            char *right = data_to_json(value->tuple.right);
            char *left = data_to_json(value->tuple.left);
            char *res = runtime_alloc(strlen(left) + strlen(right) + 4);
            res = sprintf(res, "[%s, %s]", left, right);
            free(right);
            free(left);
            return res;
        }

        case DATA_LIST: {
            char *res = runtime_alloc(value->list.count * 2);
            for (int i = 0; i < value->list.count; i++) {
                char *item = data_to_json(value->list.items[i]);
                res = sprintf(res, "%s%s", res, item);
                free(item);
                if (i < value->list.count - 1) res = sprintf(res, ", ");
            }
            return sprintf(res, "[%s]", res);
        }

        case DATA_MAP: {
            if (value->map.count == 0) return "{}";
            char *res = runtime_alloc(value->map.count * 2);
            for (int i = 0; i < value->map.count; i++) {
                struct DataValue *item = value->map.items[i];
                res = sprintf(res, "%s: %s", res, data_to_json(item));
                free(item);
                if (i < value->map.count - 1) res = sprintf(res, ", ");
            }
            return sprintf(res, "{%s}", res);
        }

        case DATA_INTEGER: {
            char *res = runtime_alloc(32);
            res = snprintf(res, "%d", value->integer);
            return res;
        } break;

        case DATA_NUMBER: {
            char *res = runtime_alloc(32);
            res = snprintf(res, "%f", value->number);
            return res;
        } break;

        case DATA_STRING: {
            char *res = runtime_alloc(strlen(value->string) + 2);
            res = snprintf(res, "\"%s\"", value->string);
            return res;
        } break;

        case DATA_BOOLEAN: {
            return value->boolean ? "true" : "false";
        } break;

        default: {
            return "invalid";
        };
    }
}



struct DataValue *data_parse_json(char *json)
{
    return NULL;
}

struct DataValue *data_unit(void)
{
    struct DataValue *value = runtime_alloc(sizeof(struct DataValue));
    value->type = DATA_UNIT;
    return value;
}

struct DataValue *data_tuple(struct DataValue *key, struct DataValue *val)
{
    struct DataValue *value = runtime_alloc(sizeof(struct DataValue));
    value->type = DATA_TUPLE;
    value->tuple.left = key;
    value->tuple.right = val;
    return value;
}

struct DataValue *data_list(struct DataValue *head, ...)
{
    if (head == NULL) {
        struct DataValue *value = runtime_alloc(sizeof(struct DataValue));
        value->type = DATA_LIST;
        value->list.count = 0;
        return value;
    }

    va_list args;
    va_start(args, head);

    struct DataValue *value= runtime_alloc(sizeof(struct DataValue));
    value->type = DATA_LIST;
    value->list.count = 0;

    slice_append(&value->list, head);

    while (true) {
        // struct DataValue item = va_arg(args, (struct DataValue));
        // if (item == NULL) break;

        // struct DataValue *entry = runtime_alloc(sizeof(struct DataValue));
        // *entry = item;

        // slice_append(&result->list, entry);
    }

    va_end(args);
    return value;
}

struct DataValue *data_map(struct DataValue *head, ...)
{
    if (head == NULL) {
        struct DataValue *value = runtime_alloc(sizeof(struct DataValue));
        value->type = DATA_MAP;
        value->map.count = 0;
        return value;
    }

    va_list args;
    va_start(args, head);

    struct DataValue *value = runtime_alloc(sizeof(struct DataValue));
    value->type = DATA_MAP;
    value->map.count = 0;

    if (head != NULL && head->type == DATA_TUPLE)
        slice_append(&value->map, &head->tuple);

    while (true) {
        // struct DataValue item = va_arg(args, (struct DataValue));
        // if (item == NULL || item->type != DATA_TUPLE) break;

        // struct DataValue *entry = runtime_alloc(sizeof(struct DataValue));
        // *entry = item;

        // slice_append(&result->map, entry);
    }

    va_end(args);
    return value;
}

struct DataValue *data_integer(int integer)
{
    struct DataValue *value = runtime_alloc(sizeof(struct DataValue));
    value->type = DATA_INTEGER;
    value->integer = integer;
    return value;
}

struct DataValue *data_number(double number)
{
    struct DataValue *value = runtime_alloc(sizeof(struct DataValue));
    value->type = DATA_NUMBER;
    value->number = number;
    return value;
}

struct DataValue *data_string(char *string)
{
    struct DataValue *value = runtime_alloc(sizeof(struct DataValue));
    value->type = DATA_STRING;
    value->string = string;
    return value;
}

struct DataValue *data_boolean(bool boolean)
{
    struct DataValue *value = runtime_alloc(sizeof(struct DataValue));
    value->type = DATA_BOOLEAN;
    value->boolean = boolean;
    return value;
}

#endif // APP_IMPLEMENTATION 
#endif // APP_HEADER