
#define MEMORY_IMPLEMENTATION
#include "../../source/app/1-memory.h"

#define RUNTIME_IMPLEMENTATION
#include "../../source/app/2-runtime.h"

#define DATA_IMPLEMENTATION
#include "../../source/app/5-data-types.h"


#include <assert.h>
#include <stdio.h>


int main(void)
{
    DataValue* empty = data_empty();
    assert(empty->type == DATA_EMPTY);

    DataValue* boolean = data_boolean(true);
    assert(boolean->boolean == true);

    DataValue* integer = data_integer(42);
    assert(integer->integer == 42);

    DataValue* decimal = data_decimal(3.14);
    assert(decimal->decimal == 3.14);

    DataValue* string = data_string("Hello World");
    assert(strcmp(string->string, "Hello World") == 0);

    DataValue* tuple = data_tuple(data_string("Hello"), data_string("World"));
    assert(strcmp(tuple->tuple->left->string, "Hello") == 0);
    assert(strcmp(tuple->tuple->right->string, "World") == 0);

    DataValue* leftTuple = data_tuple(data_string("Hello"), NULL);
    assert(strcmp(leftTuple->tuple->left->string, "Hello") == 0);
    assert(leftTuple->tuple->right != NULL);
    assert(leftTuple->tuple->right->type == DATA_EMPTY);

    DataValue* rightTuple = data_tuple(NULL, data_string("World"));
    assert(rightTuple->tuple->left != NULL);
    assert(rightTuple->tuple->left->type == DATA_EMPTY); 
    assert(strcmp(rightTuple->tuple->right->string, "World") == 0);

    DataValue* list = data_list(
        data_string("Hello"),
        data_string("World"),
    DATA_END);

    assert(list->type == DATA_LIST);
    assert(list->list->count == 2);
    assert(list->list->capacity == DATA_DEFAULT_LIST_CAPACITY);
    assert(strcmp(list->list->items[0]->string, "Hello") == 0);
    assert(strcmp(list->list->items[1]->string, "World") == 0);

    data_list_remove(list, 0);
    assert(list->list->count == 1);
    assert(list->list->capacity == DATA_DEFAULT_LIST_CAPACITY);
    assert(strcmp(list->list->items[1]->string, "World") == 0);

    DataValue* item = data_list_get(list, 1);
    assert(item->type == DATA_STRING);
    assert(strcmp(item->string, "World") == 0);

    data_list_remove(list, 1);
    assert(list->list->count == 0);
    assert(list->list->capacity == DATA_DEFAULT_LIST_CAPACITY);

    item = data_list_get(list, 0);
    assert(item != NULL);

    DataValue* dict = data_dict(
        data_entry("Hello", data_string("World")),
    DATA_END);

    assert(dict->type == DATA_DICT);
    assert(dict->dict->count == 1);
    assert(dict->dict->capacity == DATA_DEFAULT_DICT_CAPACITY);
    
    item = data_dict_get(dict, "Hello");
    assert(item != NULL);
    assert(item->type == DATA_STRING);
    assert(strcmp(item->string, "World") == 0);


    //overwrite key
    data_dict_set(dict, "Hello", data_string("World2"));
    assert(dict->dict->count == 1);
    assert(dict->dict->capacity == DATA_DEFAULT_DICT_CAPACITY);

    item = data_dict_get(dict, "Hello");
    assert(item != NULL);
    assert(item->type == DATA_STRING);
    assert(strcmp(item->string, "World2") == 0);

    //add new key
    data_dict_set(dict, "Foo", data_string("Bar"));
    assert(dict->dict->count == 2);
    assert(dict->dict->capacity == DATA_DEFAULT_DICT_CAPACITY);

    item = data_dict_get(dict, "Foo");
    assert(item != NULL);
    assert(item->type == DATA_STRING);
    assert(strcmp(item->string, "Bar") == 0);


    DataValue* bigList = data_list(DATA_END);
    for (int i = 0; i < 1000; i++) // Reallocate 5 times
        data_list_append(bigList, data_string("Hello"));
    assert(bigList->list->count == 1000);
    assert(bigList->list->capacity == 16 * DATA_DEFAULT_LIST_CAPACITY);


    data_list_remove(bigList, 0);

    // Test Conversion Functions
    assert(data_to_boolean(data_boolean(true))->boolean == true);
    assert(data_to_boolean(data_integer(1))->boolean == true);
    assert(data_to_boolean(data_decimal(1.0))->boolean == true);
    assert(data_to_boolean(data_string("true"))->boolean == true);
    assert(data_to_boolean(data_string("false"))->boolean == false);
    assert(data_to_boolean(data_tuple(data_boolean(true), data_boolean(true)))->boolean == true);
    assert(data_to_boolean(data_tuple(data_boolean(true), data_boolean(false)))->boolean == false);
    assert(data_to_boolean(data_tuple(data_boolean(false), data_boolean(false)))->boolean == false);
    assert(data_to_boolean(data_list(NULL))->boolean == false);
    assert(data_to_boolean(data_list(data_empty()))->boolean == false);
    assert(data_to_boolean(data_list(data_string("full")))->boolean == true);

    assert(data_to_integer(data_boolean(true))->integer == 1);
    assert(data_to_integer(data_integer(1))->integer == 1);
    assert(data_to_integer(data_decimal(2.0))->integer == 2);
    assert(data_to_integer(data_string("3"))->integer == 3);
    assert(data_to_integer(data_tuple(data_string("1"), data_string("2")))->integer == 3);
    assert(data_to_integer(data_list(data_string("1"), data_string("2"), NULL))->integer == 2);
    assert(data_to_integer(data_dict(data_entry("1", data_string("2")), NULL))->integer == 1);

    assert(data_to_decimal(data_boolean(true))->decimal == 1.0);    
    assert(data_to_decimal(data_integer(1))->decimal == 1.0);
    assert(data_to_decimal(data_decimal(2.0))->decimal == 2.0);
    assert(data_to_decimal(data_string("3"))->decimal == 3.0);
    assert(data_to_decimal(data_tuple(data_string("1"), data_string("2")))->decimal == 3.0);
    assert(data_to_decimal(data_list(data_string("1"), data_string("2"), NULL))->decimal == 2.0);
    assert(data_to_decimal(data_dict(data_entry("1", data_string("2")), NULL))->decimal == 1.0);


    assert(strcmp(data_to_string(data_boolean(true))->string, "true") == 0);
    assert(strcmp(data_to_string(data_boolean(false))->string,"false") == 0);
    assert(strcmp(data_to_string(data_integer(1))->string, "1") == 0);
    assert(strcmp(data_to_string(data_decimal(2.0))->string, "2.000000") == 0);
    assert(strcmp(data_to_string(data_string("foobar"))->string, "foobar") == 0);
    assert(strcmp(data_to_string(data_tuple(data_string("1"), data_string("2")))->string, "(1, 2)") == 0);
    assert(strcmp(data_to_string(data_list(data_string("1"), data_string("2"), NULL))->string, "[1, 2]") == 0);
    assert(strcmp(data_to_string(data_dict(data_entry("1", data_string("2")), NULL))->string, "{\"1\": 2}") == 0);


    printf("✅ All tests passed!\n");
    return EXIT_SUCCESS;
}
