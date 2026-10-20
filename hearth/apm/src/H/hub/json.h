#ifndef APM_JSON_H
#define APM_JSON_H

#include <stddef.h>

// A small read-only JSON reader for the hub's answers (no dependencies, like the rest of Ash).
// Ownership: json_parse() returns a tree the caller owns and releases with json_free(); every pointer the
// getters hand out is borrowed from that tree and dies with it. Nothing here is thread-safe or retained.
// Limits (a malicious or broken server must not be able to exhaust memory or the stack): 1 MiB of text,
// 32 levels of nesting. Anything else invalid makes json_parse() return NULL.
typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JsonType;

typedef struct Json {
    JsonType type;
    char *str;           // J_STR: UTF-8, NUL-terminated
    double num;          // J_NUM
    int boolean;         // J_BOOL
    size_t count;        // J_ARR / J_OBJ: number of items
    char **keys;         // J_OBJ: keys[i] belongs to items[i]
    struct Json **items; // J_ARR / J_OBJ
} Json;

Json *json_parse(const char *text);
void json_free(Json *v);

// Member of an object (NULL when `obj` is not an object or has no such key).
const Json *json_get(const Json *obj, const char *key);
// Element of an array (NULL out of range or when not an array).
const Json *json_at(const Json *arr, size_t index);
// String value, or NULL when `v` is NULL or not a string.
const char *json_str(const Json *v);
// json_str(json_get(obj, key)).
const char *json_gets(const Json *obj, const char *key);

#endif
