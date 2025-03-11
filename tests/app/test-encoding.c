

#define MEMORY_IMPLEMENTATION
#include "../../source/app/1-memory.h"

#define RUNTIME_IMPLEMENTATION
#include "../../source/app/2-runtime.h"

#define SYSTEM_IMPLEMENTATION
#include "../../source/app/3-system.h"

#define DATA_IMPLEMENTATION
#include "../../source/app/5-data-types.h"

#define ENCODING_IMPLEMENTATION
#include "../../source/app/6-encoding.h"

#include <assert.h>


int main(void)
{

    DataValue* empty = data_empty();
    printf("empty: %s\n", encoding_to_json(empty));
    assert(strcmp(encoding_to_json(empty), "null") == 0);

    DataValue* boolean = data_boolean(true);
    printf("boolean: %s\n", encoding_to_json(boolean));
    assert(strcmp(encoding_to_json(boolean), "true") == 0);

    DataValue* integer = data_integer(42);
    printf("integer: %s\n", encoding_to_json(integer));
    assert(strcmp(encoding_to_json(integer), "42") == 0);

    DataValue* decimal = data_decimal(3.14);
    printf("decimal: %s\n", encoding_to_json(decimal));
    assert(strcmp(encoding_to_json(decimal), "3.140000") == 0);

    DataValue* string = data_string("Hello World");
    printf("string: %s\n", encoding_to_json(string));
    assert(strcmp(encoding_to_json(string), "\"Hello World\"") == 0);

    DataValue* tuple = data_tuple(data_string("Hello"), data_string("World"));
    printf("tuple: %s\n", encoding_to_json(tuple));
    assert(strcmp(encoding_to_json(tuple), "[\"Hello\", \"World\"]") == 0);

    DataValue* leftTuple = data_tuple(data_string("Hello"), NULL);
    printf("leftTuple: %s\n", encoding_to_json(leftTuple));
    assert(strcmp(encoding_to_json(leftTuple), "[\"Hello\", null]") == 0);

    DataValue* rightTuple = data_tuple(NULL, data_string("World"));
    printf("rightTuple: %s\n", encoding_to_json(rightTuple));
    assert(strcmp(encoding_to_json(rightTuple), "[null, \"World\"]") == 0);

    DataValue* list = data_list(
        data_string("Hello"),
        data_string("World"),
    DATA_END);

    printf("list: %s\n", encoding_to_json(list));
    assert(strcmp(encoding_to_json(list), "[\"Hello\", \"World\"]") == 0);

    data_list_remove(list, 0);
    printf("list: %s\n", encoding_to_json(list));
    assert(strcmp(encoding_to_json(list), "[\"World\"]") == 0);

    DataValue* dict = data_dict(
        data_entry("Hello", data_string("World")),
    DATA_END);

    printf("dict: %s\n", encoding_to_json(dict));
    assert(strcmp(encoding_to_json(dict), "{\"Hello\": \"World\"}") == 0);

    data_dict_set(dict, "Foo", data_string("Bar"));
    printf("dict: %s\n", encoding_to_json(dict));
    assert(strcmp(encoding_to_json(dict), "{\"Hello\": \"World\",\"Foo\": \"Bar\"}") == 0);


    char* string_buf = "\"Hello World\"";
    string = encoding_from_json(string_buf);
    printf("string: %s\n", encoding_to_json(string));
    assert(string != NULL);
    assert(strcmp(string->string, "Hello World") == 0);

    char buf[1024] = {0};
    if (system_read_file("test-data.json", buf, sizeof(buf)) < 0) {
        printf("Failed to load test-data.json\n");
        exit(EXIT_FAILURE);
    }

    DataValue* value= encoding_from_json(buf);
    if (value == NULL) {
        printf("Failed to parse test-data.json\n");
        exit(EXIT_FAILURE);
    }

    assert(value != NULL);
    assert(data_dict_get(value, "empty")->type == DATA_EMPTY);
    assert(data_dict_get(value, "boolean")->type == DATA_BOOLEAN);
    assert(data_dict_get(value, "integer")->type == DATA_DECIMAL);
    assert(data_dict_get(value, "decimal")->type == DATA_DECIMAL);
    assert(strcmp(data_dict_get(value, "string")->string, "foobar") == 0);
    assert(data_dict_get(value, "list")->type == DATA_LIST);
    assert(data_list_get(data_dict_get(value, "list"), 0)->type == DATA_STRING);
    assert(data_list_get(data_dict_get(value, "list"), 1)->type == DATA_STRING);
    assert(data_dict_get(value, "dict")->type == DATA_DICT);
    assert(data_dict_get(data_dict_get(value, "dict"), "foo")->type == DATA_STRING);
    assert(strcmp(data_dict_get(data_dict_get(value, "dict"), "foo")->string, "bar") == 0);
}