
#define MEMORY_IMPLEMENTATION
#include "../source/1-memory.h"

#define RUNTIME_IMPLEMENTATION 
#include "../source/2-runtime.h"

#define DATA_IMPLEMENTATION
#include "../source/5-data.h"


void example_data_values()
{
    DataValue *boolean = data_boolean(true);
    runtime_logf("Data Boolean: %s\n", data_to_json(boolean));

    DataValue *integer = data_integer(42);
    runtime_logf("Data Integer: %p %s\n", integer, data_to_json(integer));

    DataValue *number = data_number(3.14);
    runtime_logf("Data Number: %p %f\n", number, number->decimal);

    DataValue *string = data_string("Hello World");
    runtime_logf("Data String: %s\n", data_to_json(string));

    DataValue *list = data_list(

        data_string("Hello"),
        data_string("World"),

    DATA_END);
    runtime_logf("Data List: %s\n", data_to_json(list));
}


int main(void)
{
    // DataValues allocated on the main process will be freed
    // when the program exits. We are still using regions to
    // store the data, but these regions are static variables.
    DataValue *empty = data_empty();
    runtime_logf("Data Empty: %s\n", data_to_json(empty));

    // DataValues allocated on a runtime process will be freed
    // when the process exits. This is useful for storing values
    // for HTTP requests, so that we can free the memory when
    // the request is done being served.
    runtime_run(example_data_values());

    return runtime_main();
}