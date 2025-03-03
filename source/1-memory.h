#ifndef MEMORY_HEADER
#define MEMORY_HEADER


////////////
// MEMORY //
////////////


#include <stddef.h>
#include <stdlib.h>


// We need a dynamic way to store data about the current state of
// our application and the processes that are running. Providing
// a few common containers for storing data, using the name list.


// Generic list of integer values, primarily used for storing
// the ids of processes in their different states
struct ListOfInt {
    int *items;
    int capacity;
    int count;
};


// Generic list of strings, primarily used for storing lists of
// null terminated strings
struct ListOfStr {
    char **items;
    int capacity;
    int count;
};


// appeds a new item to the list's memory after checking if the
// capacity is full. This will work generically for all lists.
#define list_append(list, item) ({ \
    if ((list)->count >= (list)->capacity) { \
        (list)->capacity += RUNTIME_LIST_SIZE; \
        (list)->items = realloc((list)->items, (list)->capacity * sizeof(item)); \
    } \
    (list)->items[(list)->count++] = (item); \
})


// removes an item from the list's memory and replaces it with
// the last item in the list, while also decrementing the count
#define list_remove(list, index) ({ \
    if ((index) >= (list)->count) { \
        fprintf(stderr, "[ERROR] Index out of bounds\n"); \
        exit(1); \
    } \
    (list)->items[index] = (list)->items[--(list)->count]; \
})


#define MEMORY_REGION_SIZE 1 * getpagesize()


// Memory regions are used to store data in a way that will be all
// at the same time. This is useful for storing data that we do not
// want to be cleaned up after we have rendered a template.
typedef struct MemoryRegion {

    // The next region of memory after this capacity is used
    struct MemoryRegion *next;

    // The size of the region, named count to match other structs
    int count;

    // The capacity of the region, given when region is created
    int capacity;

    // A pointer to the memory that the region is using
    char *memory;

} MemoryRegion;


// Create a new region of memory with the given capacity, that will
// dynamically grow as needed, and all be cleaned up at once.
MemoryRegion *memory_new_region(int capacity);

// Allocate memory in the current region that will be freed later.
void *memory_alloc(MemoryRegion *region, int size);

// Get the total size of region and all of its children
int memory_size(MemoryRegion *region);

// Destroy a region of memory, freeing all memory associated with it.
void memory_destroy(MemoryRegion *region);


#ifdef MEMORY_IMPLEMENTATION


MemoryRegion *memory_new_region(int capacity)
{
    // Allocating memory for the region and failing gracefully
    MemoryRegion *region = malloc(sizeof(MemoryRegion));
    if (region == NULL) return NULL;

    // Allocating memory for the region
    region->memory = malloc(capacity);
    if (region->memory == NULL) {
        free(region);
        return NULL;
    }

    // Initialize remaining fields to zero
    region->capacity = capacity;
    region->count = 0;
    region->next = NULL;
    return region;
}


void *memory_alloc(MemoryRegion *region, int size)
{
    // Base case, just malloc if we are not provided a region
    if (region == NULL) return malloc(size);

    MemoryRegion *current = region;

    // Find a region that has enough capacity to store the memory
    while (current->next != NULL && current->count + size > current->capacity) {
        current = current->next;
    }

    // If we reached the end and still don't have enough capacity
    // we need to create a new region and add it to the last region
    if (current->count + size > current->capacity) {
        int capacity = (region->capacity > size) ? region->capacity : size;
        MemoryRegion *new_region = memory_new_region(capacity);
        if (new_region == NULL) return NULL;
        current->next = new_region;
        current = new_region;
    }

    // Allocating memory for the region
    void *memory = &current->memory[current->count];
    current->count += size;
    return memory;
}


int memory_size(MemoryRegion *region)
{
    int size = 0;
    while (region != NULL) {
        size += region->capacity;
        region = region->next;
    }
    return size;
}


void memory_destroy(MemoryRegion *region)
{
    // Recursively free all child regions 
    while (region != NULL) {
        MemoryRegion *next = region->next;
        free(region->memory);
        free(region);
        region = next;
    }
}


#endif // MEMORY_IMPLEMENTATION
#endif // MEMORY_HEADER
