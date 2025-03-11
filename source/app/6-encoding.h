/** encoding.h - Provides a set of functions for encoding and decoding data
    from popular encoding formats like JSON.

    @author:  Connor McCutcheon <connor.mccutcheon95@gmail.com>
    @date:    2025-03-08
    @version  0.1.1 
    @license: MIT
*/


#ifndef ENCODING_HEADER
#define ENCODING_HEADER


// JSON functions
char* encoding_to_json(DataValue* value);

typedef struct EncodingLexer {
    char* input;
    int start;
    int count;
} EncodingLexer;


DataValue* encoding_from_json(char* input);

DataValue* encoding_next_value(EncodingLexer* lexer);

DataValue* encoding_empty_from_json(EncodingLexer*);

DataValue* encoding_boolean_from_json(EncodingLexer*);

DataValue* encoding_number_from_json(EncodingLexer*);

DataValue* encoding_string_from_json(EncodingLexer*);

DataValue* encoding_list_from_json(EncodingLexer*);

DataValue* encoding_dict_from_json(EncodingLexer*);



#ifdef ENCODING_IMPLEMENTATION


#include <ctype.h>


char* encoding_to_json(DataValue* value)
{
    switch (value->type) {
    case DATA_BOOLEAN: return value->boolean ? "true" : "false";
    case DATA_INTEGER: return runtime_sprintf("%d", value->integer);
    case DATA_DECIMAL: return runtime_sprintf("%f", value->decimal);
    case DATA_STRING: return runtime_sprintf("\"%s\"", value->string);

    case DATA_TUPLE: {
        char* left = encoding_to_json(value->tuple->left);
        char* right = encoding_to_json(value->tuple->right);
        int len = snprintf(NULL, 0, "[%s, %s]", left, right) + 1;
        char* res = runtime_alloc(len);
        snprintf(res, len, "[%s, %s]", left, right);
        return res;
    }

    case DATA_LIST: {
        char buf[2048] = {0};
        buf[0] = '[';
        for (int i = 0; i < value->list->count; i++) {
            if (i > 0) strcat(buf, ", ");
            if (value->list->items[i] == NULL) continue;
            strcat(buf, encoding_to_json(value->list->items[i]));
        }
        strcat(buf, "]");
        int count = strlen(buf);
        char* res = runtime_alloc(count + 1);
        strncpy(res, buf, count);
        res[count] = '\0';
        return res;
    }

    case DATA_DICT: {
        int count = 0;
        char buf[2048] = {0};
        count += snprintf(buf, sizeof(buf), "{");
        for (int i = 0; i < value->dict->capacity; i++) {
            if (value->dict->entries[i] == NULL) continue;
            char* key = value->dict->entries[i]->key;
            char* str = encoding_to_json(value->dict->entries[i]->value);
            count += snprintf(buf + count, sizeof(buf) - count, "\"%s\": %s,", key, str);
        }
        if (count > 1) buf[count - 1] = '}';
        else strcat(buf, "}");
        count = strlen(buf);
        char* res = runtime_alloc(count + 1);
        strncpy(res, buf, count);
        res[count] = '\0';
        return res;
    }

    default: return "null";
    }
}


// Private helper methods used for parsing.
void _encoding_skip_whitespace(EncodingLexer* lexer)
{ while (isspace(lexer->input[lexer->count])) lexer->count++; }


char _encoding_peek(EncodingLexer* lexer)
{ return lexer->input[lexer->count]; }


char _encoding_take(EncodingLexer* lexer)
{ return lexer->input[lexer->count++]; }


char* _encoding_emit(EncodingLexer* lexer)
{
    int count = lexer->count - lexer->start;
    char* value = runtime_alloc(count + 1);
    strncpy(value, lexer->input + lexer->start, count);
    value[count] = '\0';
    lexer->start = lexer->count;
    return value;
}


DataValue* encoding_from_json(char* input)
{
    EncodingLexer lexer = {.input = input, .count = 0};
    _encoding_skip_whitespace(&lexer);
    return encoding_next_value(&lexer);
}


DataValue* encoding_next_value(EncodingLexer* lexer)
{
    char next = _encoding_peek(lexer);
    if (next == 'n') return encoding_empty_from_json(lexer);
    if (next == 't' || next == 'f') return encoding_boolean_from_json(lexer);
    if (isdigit(next) || next == '-' || next == '.') return encoding_number_from_json(lexer);
    if (next == '"') return encoding_string_from_json(lexer);
    if (next == '[') return encoding_list_from_json(lexer);
    if (next == '{') return encoding_dict_from_json(lexer);
    return data_empty();
}



DataValue* encoding_string_from_json(EncodingLexer* lexer)
{
    if (_encoding_peek(lexer) != '"') return data_empty();
    _encoding_take(lexer);
    lexer->start = lexer->count;

    while (_encoding_peek(lexer) != '"' && _encoding_peek(lexer) != '\0')
        _encoding_take(lexer);

    if (_encoding_peek(lexer) != '"') return data_empty();
    
    char* str = _encoding_emit(lexer);
    _encoding_take(lexer);
    return data_string(str);
}


DataValue* encoding_list_from_json(EncodingLexer* lexer)
{
    if (_encoding_peek(lexer) != '[') return data_empty();
    _encoding_take(lexer); // Consume '['.
    
    DataValue* list = data_list(NULL);
    _encoding_skip_whitespace(lexer);
    
    if (_encoding_peek(lexer) == ']') {
        _encoding_take(lexer);
        return list;
    }
    
    while (true) {
        DataValue* item = encoding_next_value(lexer);
        data_list_append(list, item);
        _encoding_skip_whitespace(lexer);
        char next = _encoding_peek(lexer);
        if (next == ',') {
            _encoding_take(lexer);
            _encoding_skip_whitespace(lexer);
        } else if (next == ']') {
            _encoding_take(lexer);
            break;
        } else break;
    }

    return list;
}


DataValue* encoding_dict_from_json(EncodingLexer* lexer)
{
    if (_encoding_peek(lexer) != '{') return data_empty();
    _encoding_take(lexer);
    
    DataValue* dict = data_dict(NULL);
    _encoding_skip_whitespace(lexer);
    
    if (_encoding_peek(lexer) == '}') {
        _encoding_take(lexer);
        return dict;
    }
    
    while (true) {
        _encoding_skip_whitespace(lexer);

        DataValue* key_val = encoding_string_from_json(lexer);
        if (!key_val) return data_empty();
        char* key = key_val->string;
        
        _encoding_skip_whitespace(lexer);
        if (_encoding_take(lexer) != ':') return data_empty();
        
        _encoding_skip_whitespace(lexer);
        DataValue* value = encoding_next_value(lexer);
        data_dict_set(dict, key, value);
        
        _encoding_skip_whitespace(lexer);
        char next = _encoding_peek(lexer);
        if (next == ',') _encoding_take(lexer);
        else if (next == '}') {
            _encoding_take(lexer);
            break;
        } else break;
    }
    return dict;
}


DataValue* encoding_empty_from_json(EncodingLexer *lexer)
{
    if (_encoding_take(lexer) != 'n') return data_empty();
    if (_encoding_take(lexer) != 'u') return data_empty();
    if (_encoding_take(lexer) != 'l') return data_empty();
    if (_encoding_take(lexer) != 'l') return data_empty();
    _encoding_emit(lexer);
    return data_empty();
}

DataValue* encoding_boolean_from_json(EncodingLexer *lexer)
{
    switch (_encoding_peek(lexer)) {
    case 't': 
        if (_encoding_take(lexer) != 't') return data_empty();
        if (_encoding_take(lexer) != 'r') return data_empty();
        if (_encoding_take(lexer) != 'u') return data_empty();
        if (_encoding_take(lexer) != 'e') return data_empty();
        _encoding_emit(lexer);
        return data_boolean(true);
    case 'f': 
        if (_encoding_take(lexer) != 'f') return data_empty();
        if (_encoding_take(lexer) != 'a') return data_empty();
        if (_encoding_take(lexer) != 'l') return data_empty();
        if (_encoding_take(lexer) != 's') return data_empty();
        if (_encoding_take(lexer) != 'e') return data_empty();
        _encoding_emit(lexer);
        return data_boolean(false);
    default: return data_empty();
    }
}


DataValue* encoding_number_from_json(EncodingLexer* lexer)
{
    lexer->start = lexer->count;
    if (_encoding_peek(lexer) == '-') _encoding_take(lexer);
    
    while (isdigit(_encoding_peek(lexer)))
        _encoding_take(lexer);
    
    if (_encoding_peek(lexer) == '.') {
        _encoding_take(lexer);
        while (isdigit(_encoding_peek(lexer)))
            _encoding_take(lexer);
    }
    
    char* numStr = _encoding_emit(lexer);
    double value = strtod(numStr, NULL);
    return data_decimal(value);
}





#endif // ENCODING_IMPLEMENTATION
#endif // ENCODING_HEADER