
#define MEMORY_IMPLEMENTATION
#include "../../source/app/1-memory.h"

#define RUNTIME_IMPLEMENTATION
#include "../../source/app/2-runtime.h"

#include <assert.h>
#include <stdio.h>


void counter(void* arg1);
void typed_counter(int arg1);


int main(void)
{

    // Check statically allocated state
    assert(runtime_current_fiber == 0);
    assert(runtime_fibers.count == 1);
    assert(runtime_running_fibers.count == 1);
    assert(runtime_waiting_fibers.count == 0);
    assert(runtime_stopped_fibers.count == 0);
    assert(runtime_polls.count == 0);

    // You can either use the thread style way of starting
    runtime_start(counter, (void*)10);
    runtime_start(counter, (void*)20);

    // Or you can run with the macro if you compile with GCC
    runtime_run(typed_counter(30));

    // Finally, block until all processes are finished
    assert(runtime_main() == 0);


    // Test clearning the runtime memory
    


    printf("✅ All tests passed!\n");
    return EXIT_SUCCESS;
}



void counter(void* arg1)
{
    // This will behave like a stack allocated variable
    // and will be freed when the process finishes.
    long *count = runtime_alloc(sizeof(long));
    *count = (long)arg1;

    // For loops normally don't pause during execution,
    // but we can use the runtime_yield function to pause
    // the current process and allow the next process to
    // run.
    for (int i = 0; i <= *count; i++) {
        printf("count to %ld: %d\n", *count, i);
        runtime_yield();
    }
}

void typed_counter(int arg1)
{
    // This function works the same as the counter function
    // but because we are using the macro to wrap our call
    // of this function. We can more strictly check the type
    // of the argument being passed.
    for (int i = 0; i <= arg1; i++) {
        printf("count to %d: %d\n", arg1, i);
        runtime_yield();
    }
}