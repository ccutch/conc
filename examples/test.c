
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


typedef struct DataStruct {
    enum {
        EMPTY,
        BOOLEAN,
        INTEGER,
        DECIMAL,
        STRING,
        LIST,
        OBJECT,
    } type;

    union {

        bool boolean;
        int integer;
        double decimal;
        char *string;

        struct DataList {
            struct DataStruct **values;
            int count;
            int capacity;
        } list;

        // Using Raymond Hettinger's Compact Dict (aka Raymond Dict).
        //    https://www.youtube.com/watch?v=p33CVV29OG8

        struct DataDict {
            struct DataEntry {
                struct DataStruct *value;
                char *key;
            } **entries;
            int *indexes;
            int count;
            int capacity;
        } dict;

    };
} DataStruct;


// Using Ben Hoyt's hashing implementation.
//    https://benhoyt.com/writings/hash-table-in-c/


#define DATA_HASH_OFFSET 14695981039346656037UL
#define DATA_HASH_PRIME  1099511628211UL


DataStruct *empty(void)
{
    DataStruct *value = malloc(sizeof(DataStruct));
    value->type = EMPTY;
    return value;
}


unsigned long int object_hash(char *s)
{
    unsigned long int hash = DATA_HASH_OFFSET;
    for (char *c = s; *c != '\0'; c++)
        hash = DATA_HASH_PRIME * (hash ^ (unsigned long int)(unsigned char)(*c));
    return hash;
}


void object_index_entries(struct DataStruct *value)
{
    if (value->type != OBJECT || value->dict.capacity == 0)
        // Why are you here? What are you even doing?
        return;

    memset(value->dict.indexes, 0, sizeof(int) * value->dict.capacity);
    for (int i = 0; i < value->dict.count; i++) {

        struct DataEntry *entry = value->dict.entries[i];
        unsigned long int hash = object_hash(entry->key);
        int index = hash % value->dict.capacity;
        while (value->dict.indexes[index] != 0) {

            index = (5 * index + hash + 1) % value->dict.capacity;
            hash >>= 5;

        }
        value->dict.indexes[index] = i + 1;

    }
}


int object_set(struct DataStruct *value, char *key, struct DataStruct *item)
{
    if (value->type != OBJECT) return 0;

    struct DataEntry *entry = malloc(sizeof(struct DataEntry));
    entry->key = key;
    entry->value = item;

    unsigned long int hash = object_hash(key);
    int index = hash % value->dict.capacity;
    while (value->dict.indexes[index] != 0) {
        printf("searching for index %d\n", index);

        int position = value->dict.indexes[index] - 1;
        struct DataEntry *current = value->dict.entries[position];
        printf("comparing %s and %s\n", current->key, key);
        if (strcmp(current->key, key) == 0) {

            *current->value = *item;
            return index;

        } else {
            
            index = (5 * index + hash + 1) % value->dict.capacity;
            hash >>= 5;

        }

        printf("index: %d\n", index);
    }

    int position = value->dict.count;
    if (position >= value->dict.capacity) {

        value->dict.capacity *= 4;
        value->dict.entries = realloc(value->dict.entries, sizeof(struct DataEntry*) * value->dict.capacity);
        value->dict.indexes = realloc(value->dict.indexes, sizeof(int) * value->dict.capacity);

    }

    value->dict.entries[value->dict.count] = entry;
    value->dict.indexes[value->dict.count] = value->dict.count;
    value->dict.count += 1;

    value->dict.indexes[index] = position + 1;
    return index;
}


DataStruct *object_get(struct DataStruct *value, char *key)
{
    if (value->type != OBJECT || value->dict.capacity == 0) return empty();

    unsigned long int hash = object_hash(key);
    int index = hash % value->dict.capacity;
    while (value->dict.indexes[index] != 0) {

        int position = value->dict.indexes[index] - 1;
        struct DataEntry *current = value->dict.entries[position];
        if (strcmp(current->key, key) == 0) return current->value;
        else {
            
            index = (5 * index + hash + 1) % value->dict.capacity;
            hash >>= 5;

        }

    }

    return empty();
}


int main(void)
{
    DataStruct *s1 = malloc(sizeof(DataStruct));
    s1->type = BOOLEAN;
    s1->boolean = true;
    printf("s1->boolean: %d\n", s1->boolean);

    DataStruct *s2 = malloc(sizeof(DataStruct));
    s2->type = INTEGER;
    s2->integer = 42;
    printf("s2->integer: %d\n", s2->integer);

    DataStruct *s3 = malloc(sizeof(DataStruct));
    s3->type = DECIMAL;
    s3->decimal = 3.14;
    printf("s3->decimal: %f\n", s3->decimal);

    DataStruct *s4 = malloc(sizeof(DataStruct));
    s4->type = STRING;
    s4->string = "Hello World";
    printf("s4->string: %s\n", s4->string);

    DataStruct *s5 = malloc(sizeof(DataStruct));
    s5->type = LIST;
    s5->list = (struct DataList) {
        .values = malloc(sizeof(DataStruct) * 64),
        .count = 0,
        .capacity = 64,
    };

    if (s5->list.values == NULL) return 1;

    s5->list.values[0] = s1;
    s5->list.values[1] = s2;
    s5->list.values[2] = s3;
    s5->list.values[3] = s4;

    printf("s5->list[0]: %d\n", s5->list.values[0]->boolean);
    printf("s5->list[1]: %d\n", s5->list.values[1]->integer);
    printf("s5->list[2]: %f\n", s5->list.values[2]->decimal);
    printf("s5->list[3]: %s\n", s5->list.values[3]->string);

    free(s5->list.values);
    free(s5);

    DataStruct *s6 = malloc(sizeof(DataStruct));
    s6->type = OBJECT;
    s6->dict = (struct DataDict) {
        .entries = malloc(sizeof(struct DataEntry*) * 64),
        .count = 0,
        .capacity = 64,
        .indexes = malloc(sizeof(int) * 64),
    };

    object_index_entries(s6);

    object_set(s6, "Hello", s1);
    object_set(s6, "World", s2);
    object_set(s6, "World", s3);
    object_set(s6, "Foo", s3);
    object_set(s6, "Bar", s4);

    object_index_entries(s6);

    DataStruct *s12 = object_get(s6, "Hello");
    printf("s12->boolean: %d\n", s12->boolean);

    DataStruct *s22 = object_get(s6, "World");
    printf("s22->i: %d\n", s22->integer);

    DataStruct *s32 = object_get(s6, "Foo");
    printf("s32->d: %f\n", s32->decimal);

    DataStruct *s42 = object_get(s6, "Bar");
    printf("s42->s: %s\n", s42->string);
    printf("s42->type: %d\n", s42->type);

    DataStruct *s52 = object_get(s6, "Baz");
    printf("s52->s: %s\n", s52->string);
    printf("s52->type: %d\n", s52->type);

    free(s6->dict.entries);
    free(s6->dict.indexes);
    free(s6);

    DataStruct *s7 = malloc(sizeof(DataStruct));
    s7->type = OBJECT;
    s7->dict = (struct DataDict) {
        .entries = malloc(sizeof(struct DataEntry*) * 40),
        .count = 0,
        .capacity = 40,
        .indexes = malloc(sizeof(int) * 40),
    };


    for (int i = 0; i < 100; i++) {
        char key[10];
        sprintf(key, "key-%d", i);
        int pos = object_set(s7, key, s1);
        printf("s7->%s: %d\n", key, pos); 
    }

    for (int i = 0; i < 100; i++) {
        char key[10];
        sprintf(key, "key-%d", i);
        printf("s7->%s: %d\n", key, object_get(s7, key)->type);
        fflush(stdout);
    }

    free(s7->dict.entries);
    free(s7->dict.indexes);
    free(s7);

    return 0;
}