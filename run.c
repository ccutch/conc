

#define RUN_IMPLEMENTATION
#include "run.h"

static Runtime runtime = {0};

void counter(void *arg)
{
    long int n = (long int)arg;
    for (int i = 0; i < n; ++i) {
        printf("[%lu] %d\n", runtime_id(&runtime), i);
        runtime_yield(&runtime);
    }
}

int main(void)
{
    runtime_init(&runtime);

    runtime_run(&runtime, counter, (void*)10);

    runtime_run_forever(&runtime);
}