/** app.h - A framework for building high performance web apps.


    This library provides a foundation for our application server.
    It will manage memory, schedule processes, handle non-blocking
    file I/O, parse HTTP requests, and more.

    Runtime - provides a foundation for our application server.
    It will manage memory via memory pages that can be associated
    to a process. These processes will be cooperatively scheduled
    and will clean up memory when they are done.

    @author:  Connor McCutcheon
    @email:   connor.mccutcheon95@gmail.com 
    @date:    2025-03-01
    @version  0.1.0
*/


#ifndef APP_HEADER
#define APP_HEADER


#include <poll.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <assert.h>


// We need a dynamic way to store data about the current state of
// our application and the processes that are running. Providing
// a few common containers for storing data, using the name list.

#define RUNTIME_LIST_SIZE 1024
#define RUNTIME_PROC_SIZE 4096
#define RUNTIME_PAGE_SIZE 1024


// Generic list of integer values, primarily used for storing
// the ids of processes in their different states
struct ListOfInt {
    int *items;
    int capacity;
    int count;
};


// appeds a new item to the list's memory after checking if the
// capacity is full. This will work generically for all lists.
#define list_append(list, item) ({ \
    if ((list)->count >= (list)->capacity) { \
        (list)->capacity = (list)->capacity == 0 ? RUNTIME_LIST_SIZE : (list)->capacity*2; \
        (list)->items = realloc((list)->items, (list)->capacity * sizeof(item)); \
    } \
    (list)->items[(list)->count++] = (item); \
})


// removes an item from the list's memory and replaces it with
// the last item in the list, while also decrementing the count
#define list_remove(list, index) (list)->items[index] = (list)->items[--(list)->count]


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


#ifdef APP_IMPLEMENTATION 

#include <sys/mman.h>


// Setting up static variables for global state of the runtime
static int runtime_current_proc = 0;
static struct RuntimePolls runtime_polls = {0};
static struct RuntimeProcs runtime_all_procs = {0}; 
static struct ListOfInt runtime_running_procs = {0};
static struct ListOfInt runtime_waiting_procs = {0};
static struct ListOfInt runtime_stopped_procs = {0};


struct RuntimePage *runtime_new_page(int capacity)
{
    struct RuntimePage *page = malloc(sizeof(struct RuntimePage) + sizeof(char) * capacity);
    if (page == NULL) return NULL;
    page->capacity = capacity;
    page->count = 0;
    return page;
}

void runtime_destroy_page(struct RuntimePage *page)
{ free(page); }


int runtime_main(void)
{
    // We are checking both the running and waiting processes
    // to check that there are no processes that are running 
    while (runtime_running_procs.count > 1 || runtime_waiting_procs.count > 1)
        runtime_yield();
    return 0;
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
        runtime_all_procs.items[id].registers = mmap(NULL, RUNTIME_PROC_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_STACK|MAP_GROWSDOWN, -1, 0);
        runtime_all_procs.items[id].head = runtime_new_page(RUNTIME_PAGE_SIZE);
        if (runtime_all_procs.items[id].registers == MAP_FAILED) {
            fprintf(stderr, "[ERROR] Failed to allocate memory for process stack\n");
            exit(1);
        }
    }

    // Setting up end of the stack to return to cleanup 
    // when the process finishes executing.
    void **pointer = (void**)((char*)runtime_all_procs.items[id].registers + RUNTIME_PROC_SIZE - sizeof(void*));
    *(--pointer) = runtime_finish;
    *(--pointer) = fn;
    *(--pointer) = arg;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
    *(--pointer) = 0;
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
    runtime_all_procs.items[runtime_running_procs.items[0]].stack_pointer = rsp;
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
    runtime_all_procs.items[runtime_running_procs.items[0]].stack_pointer = rsp;
    struct pollfd poll = {.fd = fd, .events = POLLRDNORM};
    list_append(&runtime_waiting_procs, runtime_running_procs.items[0]);
    list_append(&runtime_polls, poll);
    list_remove(&runtime_running_procs, 0);
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
    runtime_all_procs.items[runtime_running_procs.items[0]].stack_pointer = rsp;
    struct pollfd poll = {.fd = fd, .events = POLLWRNORM};
    list_append(&runtime_waiting_procs, runtime_running_procs.items[0]);
    list_append(&runtime_polls, poll);
    list_remove(&runtime_running_procs, 0);
    runtime_continue();
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


void runtime_continue(void)
{
    // Switch to the next running coroutine
    if (runtime_polls.count > 0) {
        int timeout = runtime_running_procs.count > 1 ? 0 : -1;
        int result = poll(runtime_polls.items, runtime_polls.count, timeout);
        assert(result >= 0 && "poll failed");

        // Wake up the first sleeping coroutine
        for (int i = 0; i < runtime_polls.count;) {
            if (runtime_polls.items[i].revents) {
                int ctx = runtime_waiting_procs.items[i];
                list_remove(&runtime_polls, i);
                list_remove(&runtime_waiting_procs, i);
                list_append(&runtime_running_procs, ctx);
            } else { ++i; }
        }
    }

    // Ensure coroutines continue executing
    if (runtime_running_procs.count > 0) {
        runtime_current_proc %= runtime_running_procs.count;
        runtime_resume(runtime_all_procs.items[runtime_running_procs.items[runtime_current_proc]].stack_pointer);
    }

    // If there are no running processes, we can stop
    if (runtime_running_procs.count == 0) {
        fprintf(stderr, "[ERROR] No running processes, exiting\n");
        exit(1);
    }
}


void runtime_finish(void)
{
    if (runtime_running_procs.items[runtime_current_proc] == 0) {
        fprintf(stderr, "[ERROR] Main coroutine with id 0 should never finish\n");
        exit(1);
    }

    // Cleaning up memory pages
    struct RuntimePage *page;
    while ((page = runtime_all_procs.items[runtime_running_procs.items[runtime_current_proc]].head) != NULL) {
        runtime_all_procs.items[runtime_running_procs.items[runtime_current_proc]].head = page->next;
        runtime_destroy_page(page);
    }

    list_append(&runtime_stopped_procs, runtime_running_procs.items[runtime_current_proc]);
    list_remove(&runtime_running_procs, runtime_current_proc);

    if (runtime_polls.count > 0) {
        int timeout = runtime_running_procs.count > 1 ? 0 : -1;
        int result = poll(runtime_polls.items, runtime_polls.count, timeout); // Wait for events
        assert(result >= 0);

        for (int i = 0; i < runtime_polls.count;) {
            if (runtime_polls.items[i].revents) {
                int ctx = runtime_waiting_procs.items[i];
                list_remove(&runtime_polls, i);
                list_remove(&runtime_waiting_procs, i);
                list_append(&runtime_running_procs, ctx);
            } else {
                ++i;
            }
        }
    }

    // Ensure we don't stop if there's at least one coroutine available
    while (runtime_running_procs.count == 0 && runtime_waiting_procs.count > 0) {
        list_append(&runtime_running_procs, runtime_waiting_procs.items[0]);
        list_remove(&runtime_waiting_procs, 0);
        list_remove(&runtime_polls, 0);
    }

    if (runtime_running_procs.count > 0) {
        runtime_current_proc %= runtime_running_procs.count;
        runtime_resume(runtime_all_procs.items[runtime_running_procs.items[runtime_current_proc]].stack_pointer);
    }
}


void *runtime_alloc(int size)
{
    int process_id = runtime_running_procs.items[runtime_current_proc];
    struct RuntimeProc *proc = &runtime_all_procs.items[process_id];

    if (proc->head == NULL) {
        proc->head = runtime_new_page(RUNTIME_PAGE_SIZE);
        if (proc->head == NULL) {
            fprintf(stderr, "[ERROR] Failed to allocate memory for runtime process\n");
            exit(1);
        }
    }

    struct RuntimePage *page = proc->head;
    while (page->count + size >= page->capacity) {
        page->next = runtime_new_page(RUNTIME_PAGE_SIZE);
        if (page->next == NULL) {
            fprintf(stderr, "[ERROR] Failed to allocate memory for runtime process\n");
            exit(1);
        }
        page = page->next;
    }

    char *memory = (char*)page->memory + page->count;
    page->count += size;
    return memory;
}

#endif // APP_IMPLEMENTATION 
#endif // APP_HEADER