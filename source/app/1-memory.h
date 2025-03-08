/** memory.h - Provides a region based memory system for storing data.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-01
    @version  0.1.0 
    @license: MIT
*/


#ifndef MEMORY_HEADER
#define MEMORY_HEADER


#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>


// Record for tracking allocated blocks of memory
typedef struct MemoryBlock {
    struct MemoryBlock* next;
    char* ptr;
    int size;
} MemoryBlock;

// Based on tsoding's Arena Memory Allocator
//    https://github.com/tsoding/arena
typedef struct MemoryArena {
    struct MemoryArena* next;
    int capacity;
    int count;
    MemoryBlock *blocks;
    char memory[];
} MemoryArena;

// Create a new globally allocated MemoryArena
MemoryArena* memory_arena(int capacity);

// Count the number of blocks in the arena
int memory_block_count(MemoryArena* arena);

// Allocate a new chunk of memory in an arena
void* memory_alloc(MemoryArena* arena, int size);

// Reallocate memory to a new, larger page
void* memory_realloc(MemoryArena* arena, void* ptr, int size);

// Empty all contents of the arena
void memory_empty(MemoryArena* arena);

// Destroy an arena and all of its children
void memory_destroy(MemoryArena* arena);


#define MEMORY_DEFUALT_ARENA_SIZE getpagesize()
#define MEMORY_DEFAULT_SLICE_SIZE 100

// Macro for dynamically sized arrays
#define MEMORY_SLICE(name, type) \
    struct name {                \
        int capacity;            \
        int count;               \
        type* items;             \
    }

// Append a new item to the end of the slice
#define memory_slice_append(arena, slice, item) ({         \
    if ((slice)->count >= (slice)->capacity) {             \
        (slice)->capacity += MEMORY_DEFAULT_SLICE_SIZE;    \
        int size = (slice)->capacity * sizeof(item);       \
        (slice)->items = realloc((slice)->items, size);    \
        if ((slice)->items == NULL) {                      \
            perror("[ERROR] Failed to allocate memory\n"); \
            exit(1);                                       \
        }                                                  \
    }                                                      \
    (slice)->items[(slice)->count++] = (item);             \
})

// Remove an item from slice, and replace with last item
#define memory_slice_remove(slice, index) ({                  \
    if ((index) >= (slice)->count) {                          \
        perror("[ERROR] Index out of bounds\n");              \
        exit(1);                                              \
    }                                                         \
    (slice)->items[index] = (slice)->items[--(slice)->count]; \
})


#ifdef MEMORY_IMPLEMENTATION


#include <string.h>
#include <sys/mman.h>


MemoryArena* memory_arena(int capacity)
{
    MemoryArena* arena = mmap(NULL, sizeof(MemoryArena) + sizeof(char) * capacity,
                              PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    arena->count = 0;
    arena->capacity = capacity;
    memset(arena->memory, 0, capacity);
    arena->blocks = NULL;
    return arena;
}


int memory_block_count(MemoryArena* arena)
{
    int count = 0;
    for (MemoryBlock* alloc = arena->blocks; alloc != NULL; alloc = alloc->next)
        count++;
    return count;
}


void* memory_alloc(MemoryArena* arena, int size)
{
    // Sensible default to malloc memory
    if (arena == NULL) return malloc(size);

    // Search arena for available memory
    while (arena->count + size > arena->capacity && arena->next != NULL)
        arena = arena->next;
    
    // Allocate memory if none was found
    if (arena->count + size > arena->capacity) {

        // Allocating the right capacity
        int capacity = (arena->capacity > size) ? arena->capacity : size;
        arena->next = memory_arena(capacity*2);
        if (arena->next == NULL) return NULL;
        arena = arena->next;

    }

    // Recording allocation to the stack
    MemoryBlock* alloc = malloc(sizeof(MemoryBlock));
    alloc->ptr = &arena->memory[arena->count];
    alloc->size = size;
    alloc->next = arena->blocks;
    arena->count += size;
    arena->blocks = alloc;
    return alloc->ptr;
}


void* memory_realloc(MemoryArena* arena, void* ptr, int size)
{
    if (arena == NULL) return realloc(ptr, size);

    MemoryBlock *alloc = arena->blocks;
    while (alloc != NULL && alloc->ptr != ptr)
        alloc = alloc->next;

    if (alloc == NULL) return NULL;
    if (alloc->size == size) return ptr;
    if (alloc->size > size) return ptr;

    void* new = memory_alloc(arena, size);
    if (new == NULL) return NULL;

    char* source = (char*)ptr;
    char* sink = (char*)new;

    for (int i = 0; i < alloc->size; i++)
        sink[i] = source[i];

    return new;
}


void memory_empty(MemoryArena* arena)
{
    if (arena == NULL) return;

    // Smash the Bureaucracy
    while (arena->blocks != NULL) {

        MemoryBlock* alloc = arena->blocks;
        arena->blocks = alloc->next;
        free(alloc);

    }

    // Destroy the Bloodline
    MemoryArena* child = arena->next;
    while (child != NULL) {

        child = child->next;
        memory_destroy(child);

    }

    // And Salt the Earth
    arena->next = NULL;
    arena->count = 0;
    arena->blocks = NULL;
    memset(arena->memory, 0, arena->capacity);
}


void memory_destroy(MemoryArena* arena)
{
    if (arena == NULL) return;

    // Smash the Bureaucracy
    while (arena->blocks != NULL) {

        MemoryBlock* alloc = arena->blocks;
        arena->blocks = alloc->next;
        free(alloc);

    }

    // Destroy the Bloodline
    MemoryArena* child = arena->next;
    while (child != NULL) {

        child = child->next;
        memory_destroy(child);

    }

    // And off with the head
    munmap(arena, sizeof(MemoryArena) + sizeof(char)*arena->capacity);
}


#endif // MEMORY_IMPLEMENTATION
#endif // MEMORY_HEADER
