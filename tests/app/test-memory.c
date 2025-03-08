
#define MEMORY_IMPLEMENTATION
#include "../../source/app/1-memory.h"

#include <assert.h>
#include <stdio.h>


typedef MEMORY_SLICE(TestData, int) TestData;


int main(void)
{
    // First we are going to test the arena
    MemoryArena* arena = memory_arena(sizeof(long int));
    assert(arena != NULL);
    assert(arena->next == NULL);
    assert(arena->count == 0);
    assert(arena->capacity == sizeof(long int));
    assert(memory_block_count(arena) == 0);
    assert(arena->blocks == NULL);

    // Normal Allocation (size < capacity)
    void* data = memory_alloc(arena, sizeof(int));
    assert(data != NULL);
    assert(arena->next == NULL);
    assert(arena->count == sizeof(int));
    assert(arena->capacity == sizeof(long int));
    assert(memory_block_count(arena) == 1);
    assert(arena->blocks != NULL);

    // Large Allocation (size > capacity)
    void* big_data = memory_alloc(arena, 2000);
    assert(big_data != NULL);
    assert(arena->next != NULL);
    assert(arena->count == sizeof(int));
    assert(arena->capacity == sizeof(long int));
    assert(memory_block_count(arena) == 1);
    assert(arena->blocks != NULL);
    assert(arena->next->count == 2000);
    assert(arena->next->capacity == 4000); // size * 2
    assert(memory_block_count(arena->next) == 1);
    assert(arena->next->blocks->size == 2000);
    assert(arena->next->blocks->ptr == big_data);

    // Clean up our arena testing
    memory_empty(arena);
    assert(memory_block_count(arena) == 0);
    assert(arena->next == NULL);
    assert(arena->count == 0);
    assert(arena->capacity == sizeof(long int));
    assert(arena->blocks == NULL);

    int* b = memory_alloc(arena, sizeof(int));
    assert(b != NULL);
    assert(arena->count == sizeof(int));
    assert(arena->capacity == sizeof(long int));
    assert(memory_block_count(arena) == 1);
    assert(arena->blocks != NULL);
    assert(arena->blocks->size == sizeof(int));
    assert(arena->blocks->ptr == (void*)b);
    assert(arena->next == NULL);

    #define TEST_VALUE 42

    *b = TEST_VALUE;

    // Reallocate the memory
    int* b2 = memory_realloc(arena, (void*)b, 60000);
    assert(b2 != NULL);
    assert(arena->count == sizeof(int));
    assert(arena->capacity == sizeof(long int));
    assert(memory_block_count(arena) == 1);
    assert(arena->blocks != NULL);
    assert(arena->blocks->size == sizeof(int));
    assert(arena->blocks->ptr == (void*)b);
    assert(arena->next != NULL);
    assert(arena->next->count == 60000);
    assert(arena->next->capacity == 120000);
    assert(memory_block_count(arena->next) == 1);
    assert(arena->next->blocks != NULL);
    assert(arena->next->blocks->size == 60000);
    assert(arena->next->blocks->ptr == (void*)b2);

    assert(*b2 == TEST_VALUE);
    *b2 = TEST_VALUE * 2;
    assert(*b != *b2);

    // Clean up our arena testing
    memory_empty(arena);
    assert(memory_block_count(arena) == 0);
    assert(arena->next == NULL);
    assert(arena->count == 0);
    assert(arena->capacity == sizeof(long int));
    assert(arena->blocks == NULL);

    // Allocate a slice of memory,
    TestData *slice = memory_alloc(arena, sizeof(TestData));
    assert(slice != NULL);
    assert(arena->count == 0);
    assert(arena->capacity == sizeof(long int));
    assert(memory_block_count(arena) == 0);
    assert(arena->blocks == NULL);
    // that is larger than the arena 
    assert(arena->next != NULL);
    assert(arena->next->count == sizeof(TestData));
    assert(arena->next->capacity == sizeof(TestData)*2);
    assert(memory_block_count(arena->next) == 1);
    assert(arena->next->blocks->size == sizeof(TestData));
    assert(arena->next->blocks->ptr == (void*)slice);

    // Add some items to the slice
    memory_slice_append(arena, slice, 1);
    memory_slice_append(arena, slice, 2);
    memory_slice_append(arena, slice, 3);

    // Check that the slice is correct
    assert(slice->count == 3);
    assert(slice->capacity == MEMORY_DEFAULT_SLICE_SIZE);
    assert(slice->items[0] == 1);
    assert(slice->items[1] == 2);
    assert(slice->items[2] == 3);

    // Remove an item from the slice
    memory_slice_remove(slice, 1);
    assert(slice->count == 2);
    assert(slice->capacity == MEMORY_DEFAULT_SLICE_SIZE);
    assert(slice->items[0] == 1);
    assert(slice->items[1] == 3);

    // Clearing out the slice
    memory_slice_remove(slice, 0);
    memory_slice_remove(slice, 0);
    assert(slice->count == 0);
    assert(slice->capacity == MEMORY_DEFAULT_SLICE_SIZE);

    slice->count = MEMORY_DEFAULT_SLICE_SIZE;
    memory_slice_append(arena, slice, TEST_VALUE);
    assert(slice->count == MEMORY_DEFAULT_SLICE_SIZE+1);
    assert(slice->capacity == MEMORY_DEFAULT_SLICE_SIZE*2);

    slice->count = MEMORY_DEFAULT_SLICE_SIZE*2;
    memory_slice_append(arena, slice, TEST_VALUE);
    assert(slice->count == MEMORY_DEFAULT_SLICE_SIZE*2+1);
    assert(slice->capacity == MEMORY_DEFAULT_SLICE_SIZE*3);

    // Destroy it all and clean up
    memory_destroy(arena);

    printf("✅ All tests passed!\n");
    return EXIT_SUCCESS;
}