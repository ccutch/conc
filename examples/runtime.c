
#define MEMORY_IMPLEMENTATION
#include "../source/1-memory.h"

#define RUNTIME_IMPLEMENTATION
#include "../source/2-runtime.h"


void counter(void *arg)
{
    // This will behave like a stack allocated variable
    // and will be freed when the process finishes.
    long *count = runtime_alloc(sizeof(long));
    *count = (long)arg;

    // For loops normally don't pause during execution,
    // but we can use the runtime_yield function to pause
    // the current process and allow the next process to
    // run.
    for (int i = 0; i <= *count; i++) {
        printf("count to %ld: %d\n", *count, i);
        runtime_yield();
    }
}

void typed_counter(int count)
{
    // This function works the same as the counter function
    // but because we are using the macro to wrap our call
    // of this function. We can more strictly check the type
    // of the argument being passed.
    for (int i = 0; i <= count; i++) {
        printf("count to %d: %d\n", count, i);
        runtime_yield();
    }
}


int main(void)
{
    // You can either use the thread style way of starting
    runtime_start(counter, (void*)10);
    runtime_start(counter, (void*)20);

    // Or you can run with the macro if you are using gcc
    runtime_run(typed_counter(30));

    // Finally, block until all processes are finished
    return runtime_main();
}