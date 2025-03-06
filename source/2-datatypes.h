/** datatypes.h - Provides a shallow layer of abstraction on top of json like 
                  data structures to allow for reflection and serialization.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-03
    @version  0.1.0 
    @license: MIT
*/


#ifndef DATA_HEADER
#define DATA_HEADER


typedef struct DataValue {
    enum {
        // Our "empty" DataValue
        DATA_EMPTY,

        // Boolean DataValue
        DATA_BOOLEAN,

        // Integer DataValue
        DATA_INTEGER,

        // Decimal DataValue
        DATA_NUMBER,

        // String DataValue
        DATA_STRING,

        // List of DataValues 
        DATA_LIST,

        // Hash table of DataValues
        DATA_OBJECT,
    } type;

    union {
        bool boolean;
        int integer;
        float decimal;
        char *string;

        struct DataList {
            struct DataList *next;
            struct DataValue **values;
            int count;
            int capacity;
        } list;

        struct DataObject {
            struct DataEntry {
                struct DataValue *value;
                char *key;
            } *items;

            int *indexes;
            int count;
            int capacity;
        } object;
    };
} DataValue;


// Construct a new empty DataValue
DataValue *data_empty(void);

// Construct a new DataValue from a boolean
DataValue *data_boolean(bool boolean);

// Construct a new DataValue from an integer
DataValue *data_integer(int integer);

// Construct a new DataValue from a decimal
DataValue *data_number(float decimal);

// Construct a new DataValue from a string
DataValue *data_string(char* string);

// Construct a new DataValue list container
DataValue *data_list(DataValue *head, ...);

// Append a DataValue to an existing list
int data_list_append(DataValue *list, DataValue *value);

// Construct a new DataValue hash map container
DataValue *data_object(DataValue *head, ...);

// Transform a DataValue into a DataString value
DataValue *data_to_string(DataValue *value);

// Marshal a DataValue into a JSON string
char* data_to_json(DataValue *value);

// Convienence macro for ending a dynamic data structure
#define DATA_END data_empty()


#ifdef DATA_IMPLEMENTATION
    

DataValue *data_empty(void)
{
    DataValue *value = runtime_alloc(sizeof(DataValue));
    value->type = DATA_EMPTY;
    return value;
}

DataValue *data_boolean(bool boolean)
{
    DataValue *value = runtime_alloc(sizeof(DataValue));
    value->type = DATA_BOOLEAN;
    value->boolean = boolean;
    return value;
}


DataValue *data_integer(int integer)
{
    DataValue *value = runtime_alloc(sizeof(DataValue));
    value->type = DATA_INTEGER;
    value->integer = integer;
    return value;
}


DataValue *data_number(float decimal)
{
    DataValue* value = runtime_alloc(sizeof(DataValue));
    value->type = DATA_NUMBER;
    value->decimal = decimal;
    return value;
}


DataValue *data_string(char* string)
{
    DataValue* value = runtime_alloc(sizeof(DataValue));
    value->type = DATA_STRING;
    value->string = string;
    return value;
}


DataValue *data_list(DataValue *head, ...)
{
    DataValue* value = runtime_alloc(sizeof(DataValue));
    value->type = DATA_LIST;
    value->list = (struct DataList) {
        .next = NULL,
        .values =runtime_alloc(sizeof(DataValue*) * 64),
        .count = 0,
        .capacity = 64,
    };

    if (head->type == DATA_EMPTY) return value;

    va_list args;
    va_start(args, head);

        data_list_append(value, head);
        while (true) {
            DataValue *item = va_arg(args, DataValue*);
            if (item->type == DATA_EMPTY) break;
            data_list_append(value, item);
        }

    va_end(args);
    return value;
}


int data_list_append(DataValue *list, DataValue *value)
{
    if (list->type != DATA_LIST) return 0;

    struct DataList *current = &list->list;
    if (list == NULL) return 0;

    while (current->next != NULL && current->count >= current->capacity)
        current = current->next;

    if (current->count >= current->capacity) {
        struct DataList *next = runtime_alloc(sizeof(struct DataList));
        next->values = runtime_alloc(sizeof(DataValue*) * current->capacity);
        next->count = 0;
        next->capacity = current->capacity;
        current->next = next;
        current = next;
    }

    current->values[current->count++] = value;
    return current->count - 1;
}


DataValue *data_object(DataValue *head, ...)
{
    (void)head;
    DataValue *value = runtime_alloc(sizeof(DataValue));
    
    value->type = DATA_OBJECT;
    // TODO
    return value;
}


DataValue *data_to_string(DataValue *value)
{
    switch (value->type)
    {
    case DATA_EMPTY: return data_string("");
    case DATA_BOOLEAN: return value->boolean ? data_string("true") : data_string("false");
    case DATA_INTEGER: return data_string(runtime_sprintf("%d", value->integer));
    case DATA_NUMBER: return data_string(runtime_sprintf("%f", value->decimal));
    case DATA_LIST: {
        char* res = runtime_alloc(2048);
        res[0] = '[';
        res[1] = '\0';

        for (int i = 0; i < value->list.count; i++) {
            strcat(res, data_to_string(value->list.values[i])->string);
            if (i < value->list.count - 1) strcat(res, ", ");
        }

        strcat(res, "]");
        return data_string(res);
    };

    case DATA_OBJECT: {
        char* res = runtime_alloc(2048);
        res[0] = '{';
        res[1] = '\0';

        for (int i = 0; i < value->object.count; i++) {
            char* key = value->object.items[i].key;
            char* content = data_to_string(value->object.items[i].value)->string;
            strcat(res, runtime_sprintf("\"%s\": %s", key, content));
            if (i < value->object.count - 1) strcat(res, ", ");
        }

        strcat(res, "}");
        return data_string(res);
    };

    // Default case is treated as string value
    default: return value;
    }
}


char* data_to_json(DataValue *value)
{
    switch (value->type)
    {
    case DATA_BOOLEAN: return value->boolean ? "true" : "false";
    case DATA_INTEGER: return runtime_sprintf("%d", value->integer);
    case DATA_NUMBER: return runtime_sprintf("%f", value->decimal);
    case DATA_STRING: return runtime_sprintf("\"%s\"", value->string);
    case DATA_LIST: {
        char* res = runtime_alloc(2048);
        res[0] = '[';
        res[1] = '\0';

        for (int i = 0; i < value->list.count; i++) {
            strcat(res, data_to_json(value->list.values[i]));
            if (i < value->list.count - 1) strcat(res, ", ");
        }

        strcat(res, "]");
        return res;
    };

    case DATA_OBJECT: {
        char* res = runtime_alloc(2048);
        res[0] = '{';
        res[1] = '\0';

        for (int i = 0; i < value->object.count; i++) {
            char* key = value->object.items[i].key;
            char* content = data_to_json(value->object.items[i].value);
            strcat(res, runtime_sprintf("\"%s\": %s", key, content));
            if (i < value->object.count - 1) strcat(res, ", ");
        }

        strcat(res, "}");
        return res;
    };

    // Default case is treated as empty value
    default: return "null";
    }
}


#endif // DATA_IMPLEMENTATION
#endif // DATA_HEADER
