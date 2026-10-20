#include <stdlib.h>
#include <string.h>
#include "H/hub/json.h"

#define MAX_TEXT (1u << 20)
#define MAX_DEPTH 32

typedef struct { const char *p; int depth; int failed; } Parser;

static Json *node(JsonType type) {
    Json *v = calloc(1, sizeof(*v));
    if (v) v->type = type;
    return v;
}

static void skip_ws(Parser *ps) {
    while (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r') ps->p++;
}

static int hex4(const char *s, unsigned *out) {
    unsigned v = 0;
    for (int i = 0; i < 4; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return 0;
    }
    *out = v;
    return 1;
}

static size_t utf8(unsigned cp, char *out) {
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    if (cp < 0x800) { out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    if (cp < 0x10000) { out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
    out[0] = (char)(0xF0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 0x3F)); out[2] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

// Reads a string literal at ps->p (which is on the opening quote); returns a malloc'd decoded copy or NULL.
static char *read_string(Parser *ps) {
    const char *s = ps->p + 1;
    size_t cap = strlen(s) + 1, n = 0; // decoded text is never longer than the source
    char *out = malloc(cap);
    if (!out) return NULL;
    while (*s && *s != '"') {
        if (*s != '\\') { out[n++] = *s++; continue; }
        s++;
        unsigned cp;
        switch (*s) {
            case '"': case '\\': case '/': out[n++] = *s++; break;
            case 'b': out[n++] = '\b'; s++; break;
            case 'f': out[n++] = '\f'; s++; break;
            case 'n': out[n++] = '\n'; s++; break;
            case 'r': out[n++] = '\r'; s++; break;
            case 't': out[n++] = '\t'; s++; break;
            case 'u':
                if (!hex4(s + 1, &cp)) { free(out); return NULL; }
                s += 5;
                if (cp >= 0xD800 && cp <= 0xDBFF) { // a surrogate pair is one code point
                    unsigned lo;
                    if (s[0] != '\\' || s[1] != 'u' || !hex4(s + 2, &lo) || lo < 0xDC00 || lo > 0xDFFF) { free(out); return NULL; }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    s += 6;
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) { free(out); return NULL; }
                n += utf8(cp, out + n);
                break;
            default: free(out); return NULL;
        }
    }
    if (*s != '"') { free(out); return NULL; }
    out[n] = '\0';
    ps->p = s + 1;
    return out;
}

static Json *parse_value(Parser *ps);

static Json *parse_container(Parser *ps, int is_obj) {
    char close = is_obj ? '}' : ']';
    Json *v = node(is_obj ? J_OBJ : J_ARR);
    if (!v) return NULL;
    ps->p++;
    skip_ws(ps);
    if (*ps->p == close) { ps->p++; return v; }
    for (;;) {
        char *key = NULL;
        skip_ws(ps);
        if (is_obj) {
            if (*ps->p != '"' || !(key = read_string(ps))) goto fail;
            skip_ws(ps);
            if (*ps->p++ != ':') { free(key); goto fail; }
        }
        Json *item = parse_value(ps);
        Json **items = item ? realloc(v->items, (v->count + 1) * sizeof(*items)) : NULL;
        char **keys = (item && is_obj) ? realloc(v->keys, (v->count + 1) * sizeof(*keys)) : NULL;
        if (!items || (is_obj && !keys)) { free(key); json_free(item); if (items) v->items = items; goto fail; }
        v->items = items;
        if (is_obj) { v->keys = keys; v->keys[v->count] = key; }
        v->items[v->count++] = item;
        skip_ws(ps);
        if (*ps->p == ',') { ps->p++; continue; }
        if (*ps->p == close) { ps->p++; return v; }
        goto fail;
    }
fail:
    json_free(v);
    return NULL;
}

static Json *parse_value(Parser *ps) {
    skip_ws(ps);
    if (ps->depth >= MAX_DEPTH) return NULL;
    Json *v = NULL;
    if (*ps->p == '{' || *ps->p == '[') {
        ps->depth++;
        v = parse_container(ps, *ps->p == '{');
        ps->depth--;
    } else if (*ps->p == '"') {
        char *s = read_string(ps);
        if (s && (v = node(J_STR))) v->str = s; else free(s);
    } else if (strncmp(ps->p, "true", 4) == 0 || strncmp(ps->p, "false", 5) == 0) {
        int t = *ps->p == 't';
        if ((v = node(J_BOOL))) v->boolean = t;
        ps->p += t ? 4 : 5;
    } else if (strncmp(ps->p, "null", 4) == 0) {
        v = node(J_NULL);
        ps->p += 4;
    } else if (*ps->p == '-' || (*ps->p >= '0' && *ps->p <= '9')) {
        char *end;
        double d = strtod(ps->p, &end);
        if (end != ps->p && (v = node(J_NUM))) v->num = d;
        ps->p = end;
    }
    return v;
}

Json *json_parse(const char *text) {
    if (!text || strlen(text) > MAX_TEXT) return NULL;
    Parser ps = { text, 0, 0 };
    Json *v = parse_value(&ps);
    skip_ws(&ps);
    if (v && *ps.p != '\0') { json_free(v); return NULL; } // trailing garbage
    return v;
}

void json_free(Json *v) {
    if (!v) return;
    for (size_t i = 0; i < v->count; i++) {
        json_free(v->items[i]);
        if (v->keys) free(v->keys[i]);
    }
    free(v->items);
    free(v->keys);
    free(v->str);
    free(v);
}

const Json *json_get(const Json *obj, const char *key) {
    if (!obj || obj->type != J_OBJ) return NULL;
    for (size_t i = 0; i < obj->count; i++) if (strcmp(obj->keys[i], key) == 0) return obj->items[i];
    return NULL;
}

const Json *json_at(const Json *arr, size_t index) {
    return (arr && arr->type == J_ARR && index < arr->count) ? arr->items[index] : NULL;
}

const char *json_str(const Json *v) { return (v && v->type == J_STR) ? v->str : NULL; }
const char *json_gets(const Json *obj, const char *key) { return json_str(json_get(obj, key)); }
