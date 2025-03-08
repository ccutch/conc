/** data.h - Dynamic data types for our application.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/


#ifndef DATA_HEADER
#define DATA_HEADER


#include <stdbool.h>


// TODO: test these values
#define DATA_DEFAULT_LIST_CAPACITY 64
#define DATA_DEFAULT_DICT_CAPACITY 40


typedef struct DataValue {
    enum {
        DATA_EMPTY,
        DATA_BOOLEAN,
        DATA_INTEGER,
        DATA_NUMBER,
        DATA_STRING,
        DATA_TUPLE,
        DATA_LIST,
        DATA_DICT,
    } type;

    union {

        bool boolean;
        int integer;
        double decimal;
        char* string;

        struct DataTuple {
            struct DataValue* left;
            struct DataValue* right;
        }* tuple;

        struct DataList{
            struct DataValue** items;
            struct DataIndex {
                struct DataIndex* next;
                int index;
            }* available;
            int count;
            int capacity;
        }* list;

        struct DataDict {
            struct DataEntry {
                struct DataValue* value;
                char* key;
                unsigned long int hash;
            } **entries;
            struct DataIndex* indexes;
            int count;
            int capacity;
        }* dict;

    };
} DataValue;


// Data Type Constructors

DataValue* data_empty(void);

DataValue* data_boolean(bool boolean);

DataValue* data_integer(int integer);

DataValue* data_number(double decimal);

DataValue* data_string(char* string);

DataValue* data_tuple(DataValue* left, DataValue* right);

DataValue* data_list(DataValue* head, ...);

DataValue* data_dict(struct DataEntry* head, ...);


// Data List Functions

int data_list_prepend(DataValue* value, DataValue* item);

int data_list_append(DataValue* value, DataValue* item);

DataValue* data_list_get(DataValue* value, int index);

DataValue* data_list_remove(DataValue* value, int index);


// Data Dict Functions

struct DataEntry* data_entry(char* key, DataValue* value);

unsigned long int data_hash(char* s);

void data_dict_set(DataValue* value, char* key, DataValue* item);

DataValue* data_dict_get(DataValue* value, char* key);


// Data Helper Macros
#define DATA_END NULL


#ifdef DATA_IMPLEMENTATION


DataValue* data_empty(void)
{
    DataValue* value = runtime_alloc(sizeof(DataValue));
    value->type = DATA_EMPTY;
    return value;
}


DataValue* data_boolean(bool boolean)
{
    DataValue* value = data_empty();
    value->type = DATA_BOOLEAN;
    value->boolean = boolean;
    return value;
}


DataValue* data_integer(int integer)
{
    DataValue* value = data_empty();
    value->type = DATA_INTEGER;
    value->integer = integer;
    return value;
}


DataValue* data_number(double decimal)
{
    DataValue* value = data_empty();
    value->type = DATA_NUMBER;
    value->decimal = decimal;
    return value;
}


DataValue* data_string(char* string)
{
    DataValue* value = data_empty();
    value->type = DATA_STRING;
    value->string = string;
    return value;
}


DataValue* data_tuple(DataValue* left, DataValue* right)
{
    DataValue* value = data_empty();
    value->type = DATA_TUPLE;
    value->tuple = runtime_alloc(sizeof(struct DataValue*) * 2);
    if (left == NULL) left = data_empty();
    value->tuple->left = left;
    if (right == NULL) right = data_empty();
    value->tuple->right = right;
    return value;
}


DataValue* data_list(DataValue* head, ...)
{
    DataValue* value = data_empty();
    value->type = DATA_LIST;

    value->list = runtime_alloc(sizeof(struct DataList));
    value->list->items = runtime_alloc(sizeof(DataValue*) * DATA_DEFAULT_LIST_CAPACITY);
    memset(value->list->items, 0, sizeof(DataValue*) * DATA_DEFAULT_LIST_CAPACITY);
    value->list->count = 0;
    value->list->capacity = DATA_DEFAULT_LIST_CAPACITY;
    value->list->available = NULL;

    va_list args;
    va_start(args, head);
        if (head != NULL) data_list_append(value, head);
        while (true) {
            DataValue* item = va_arg(args, DataValue*);
            if (item == NULL) break;
            if (item->type > DATA_DICT) continue;
            data_list_append(value, item);
        }
    va_end(args);

    return value;
}


DataValue* data_dict(struct DataEntry* head, ...)
{
    DataValue* value = data_empty();
    value->type = DATA_DICT;

    value->dict = runtime_alloc(sizeof(struct DataDict));
    value->dict->entries = runtime_alloc(sizeof(struct DataEntry*) * DATA_DEFAULT_DICT_CAPACITY);
    value->dict->count = 0;
    value->dict->capacity = DATA_DEFAULT_DICT_CAPACITY;
    value->dict->indexes = NULL;

    if (head == NULL) return value;

    va_list args;
    va_start(args, head);
        data_dict_set(value, head->key, head->value);
        while (true) {
            struct DataEntry* item = va_arg(args, struct DataEntry*);
            if (item == NULL) break;
            data_dict_set(value, item->key, item->value);
        }
    va_end(args);
    return value;
}


int data_list_prepend(DataValue* value, DataValue* item)
{
    if (value->type != DATA_LIST) return 0;

    struct DataList* list = value->list;
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->items = runtime_alloc(sizeof(DataValue*) * list->capacity);
    }

    if (list->items[0] != NULL)
        for (int i = list->capacity - 1; i > 0; i--) {
            list->items[i] = list->items[i - 1];
        }

    list->items[0] = item;
    list->count += 1;
    return 0;
}


int data_list_append(DataValue* value, DataValue* item)
{
    if (value->type != DATA_LIST) return 0;

    struct DataList* list = value->list;
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->items = runtime_alloc(sizeof(DataValue*) * list->capacity);
    }

    int index;
    if (list->available != NULL) {
        index = list->available->index;
        list->available = list->available->next;
    } else {
        index = list->count++;
    }

    list->items[index] = item;
    return list->count - 1;
}


DataValue* data_list_remove(DataValue* value, int index)
{
    if (value == NULL) return data_empty();
    if (value->type != DATA_LIST) return data_empty();

    struct DataList* list = value->list;
    if (list == NULL) return data_empty();

    list->available = runtime_alloc(sizeof(struct DataIndex));
    list->available->index = index;
    list->available->next = list->available;

    DataValue* item = list->items[index];
    list->items[index] = NULL;
    list->count -= 1;
    return item;
}


DataValue* data_list_get(DataValue* value, int index)
{
    if (value == NULL) return data_empty();
    if (value->type != DATA_LIST) return data_empty();

    struct DataList* list = value->list;
    if (list == NULL) return data_empty();

    DataValue* item = list->items[index];
    if (item == NULL) return data_empty();

    return item;
}


void data_print_list(DataValue* value)
{
    if (value == NULL) return;
    if (value->type != DATA_LIST) return;

    struct DataList* list = value->list;
    if (list == NULL) return;

    for (int i = 0; i < list->count; i++) {
        printf("%d: ", i);
        if (list->items[i] == NULL) continue;
        printf("type: %d\n", list->items[i]->type);
        // if (list->items[i]->type == DATA_EMPTY) continue;
        // printf("%s", list->items[i]->string);
        printf("\n");
    }
}

void data_print_dict(DataValue* value)
{
    if (value == NULL) return;
    if (value->type != DATA_DICT) return;
    struct DataDict* dict = value->dict;

    for (int i = 0; i < dict->count; i++) {
        if (dict->entries[i] == NULL) continue;
        printf("%s: ", dict->entries[i]->key);
        printf("%p", dict->entries[i]->value);
        printf("\n");
    }
}


struct DataEntry* data_entry(char* key, DataValue* value)
{
    struct DataEntry* entry = runtime_alloc(sizeof(struct DataEntry));
    entry->key = key;
    entry->value = value;
    entry->hash = data_hash(key);
    return entry;
}


// Using Ben Hoyt's hashing implementation.
//    https://benhoyt.com/writings/hash-table-in-c/


#define DATA_HASH_OFFSET 14695981039346656037UL
#define DATA_HASH_PRIME  1099511628211UL


unsigned long int data_hash(char *s)
{
    unsigned long int hash = DATA_HASH_OFFSET;
    for (char *c = s; *c != '\0'; c++)
        hash = DATA_HASH_PRIME * (hash ^ (unsigned long int)(unsigned char)(*c));
    return hash;
}


void data_dict_set(DataValue* value, char* key, DataValue* item)
{
    if (value->type != DATA_DICT) return;

    struct DataDict* dict = value->dict;
    struct DataEntry* entry = data_entry(key, item);

    unsigned long long int hash = entry->hash;
    struct DataIndex* index = runtime_alloc(sizeof(struct DataIndex));
    index->next = dict->indexes;
    index->index = hash % dict->capacity;
    while (dict->entries[index->index] != NULL) {
        if (dict->entries[index->index]->hash == hash) {
            dict->entries[index->index]->value = item;
            return;
        }
        index->index = (5 * index->index + hash + 1) % dict->capacity;
        hash >>= 5;
    }

    dict->entries[index->index] = entry;
    dict->indexes = index;
    dict->count += 1;
    return;
}


DataValue* data_dict_get(DataValue* value, char* key)
{
    if (value->type != DATA_DICT) return data_empty();

    struct DataDict* dict = value->dict;
    unsigned long long int hash = data_hash(key);
    int index = hash % dict->capacity;
    while (dict->entries[index] != NULL) {
        if (dict->entries[index]->hash == hash) 
            return dict->entries[index]->value;
        index = (5 * index + hash + 1) % dict->capacity;
        hash >>= 5;
    }

    return dict->entries[index]->value;
}


#endif // DATA_IMPLEMENTATION
#endif // DATA_HEADER