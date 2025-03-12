#define APP_IMPLEMENTATION


#include "../../source/lite/0-prelude.h"
#include "../../source/lite/1-runtime.h"

#include <stdio.h>


void counter(void* arg1);
void typed_counter(int arg1);


int main(void)
{
    int *ptr = runtime_alloc(10);
    *ptr = 40;
    printf("size: %d\n", runtime_memory_size());
    runtime_alloc(10);
    printf("size: %d\n", runtime_memory_size());
    ptr = runtime_realloc(ptr, 100);
    printf("size: %d\n", runtime_memory_size());
    printf("%d\n", *ptr);

    runtime_start(counter, (void*)10);
    runtime_start(counter, (void*)20);
    
    runtime_run(typed_counter(30));

    runtime_main();
    printf("✅ All tests passed!\n");
    return 0;
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
    runtime_start(counter, (void*)(long)arg1-5);

    // This function works the same as the counter function
    // but because we are using the macro to wrap our call
    // of this function. We can more strictly check the type
    // of the argument being passed.
    for (int i = 0; i <= arg1; i++) {
        printf("count to %d: %d\n", arg1, i);
        runtime_yield();
    }
}