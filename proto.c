#define PROTO_IMPLEMENTATION
#include "proto.h"


void counter(int count)
{
    for (int i = 0; i <= count; i++) {
        printf("count: %d\n", i);
        runtime_yield();
    }
}

int main(void)
{
    runtime_init();
    runtime_run(counter(10));
    runtime_run(counter(20));
    runtime_main();
    return 0;
}