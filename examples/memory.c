/** memory.c - Example usage of the memory module.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-01
    @version  0.1.0 
    @license: MIT
*/

#include <stdio.h>
#include <assert.h>

#define MEMORY_IMPLEMENTATION
#include "../source/1-memory.h"

int main(void)
{
    MemoryRegion *region = memory_new_region(1024);
    assert(region != NULL);
    assert(region->count == 0);
    assert(region->capacity == 1024);
    assert(region->next == NULL);

    int *data = memory_alloc(region, sizeof(int));
    assert(data != NULL);
    assert(region->count == sizeof(int));
    
    void *big_data = memory_alloc(region, 2000);
    assert(big_data != NULL);
    
    assert(region->next != NULL);  
    assert(region->next->count == 2000);
    assert(region->next->capacity >= 2000); 

    int *more_data = memory_alloc(region, sizeof(int));
    assert(more_data != NULL);
    
    assert(region->count == sizeof(int) * 2);

    memory_destroy(region);

    printf("✅ All tests passed!\n");
    return 0;
}
