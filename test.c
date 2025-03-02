#define APP_IMPLEMENTATION
#include "refactor/app.h"


void counter(void *arg)
{
    int count = (int)arg;
    for (int i = 0; i <= count; i++) {
        printf("count to %d: %d\n", count, i);
        runtime_yield();
    }
}

int main(void)
{
    runtime_start(counter, (void*)10);
    runtime_start(counter, (void*)20);
    return runtime_main();
}