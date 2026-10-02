#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "builtins/internal.h"
#include "vm/vm.h"

VMValue vm_call_builtin_string(int id, VMValue *args, int argc) {
    switch (id) {
        case 15: { // split
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("split(str, delim) expected\n"); }
            const char *s = args[0].str; const char *delim = args[1].str;
            int dlen = (int)strlen(delim);
            VMArray *result = vm_array_new();
            if (dlen == 0) {
                int slen = (int)strlen(s);
                for (int i = 0; i < slen; i++) { char *c = malloc(2); c[0] = s[i]; c[1] = '\0'; vm_array_push(result, vm_str(c)); }
                return vm_array_val(result);
            }
            const char *cur = s; const char *found;
            while ((found = strstr(cur, delim)) != NULL) {
                int len = (int)(found - cur);
                char *piece = malloc(len + 1); memcpy(piece, cur, len); piece[len] = '\0';
                vm_array_push(result, vm_str(piece));
                cur = found + dlen;
            }
            char *piece = malloc(strlen(cur) + 1); strcpy(piece, cur);
            vm_array_push(result, vm_str(piece));
            return vm_array_val(result);
        }
        case 16: { // join
            if (argc != 2 || args[0].type != VM_ARRAY || args[1].type != VM_STR) { vm_runtime_error("join(array, delim) expected\n"); }
            int total = 0; int dlen = (int)strlen(args[1].str);
            for (int i = 0; i < args[0].array->count; i++) {
                if (args[0].array->items[i].type != VM_STR) { vm_runtime_error("join() expects an array of strings\n"); }
                total += (int)strlen(args[0].array->items[i].str);
                if (i < args[0].array->count - 1) total += dlen;
            }
            char *buf = malloc(total + 1); buf[0] = '\0';
            for (int i = 0; i < args[0].array->count; i++) {
                strcat(buf, args[0].array->items[i].str);
                if (i < args[0].array->count - 1) strcat(buf, args[1].str);
            }
            return vm_str(buf);
        }
        case 17: { // substring
            if (argc != 3 || args[0].type != VM_STR) { vm_runtime_error("substring(str, start, end) expected\n"); }
            int slen = (int)strlen(args[0].str);
            int start = (int)args[1].number; int end = (int)args[2].number;
            if (start < 0) start = 0;
            if (end > slen) end = slen;
            if (start > end) start = end;
            char *buf = malloc(end - start + 1); memcpy(buf, args[0].str + start, end - start); buf[end - start] = '\0';
            return vm_str(buf);
        }
        case 18: { // indexOf
            if (argc != 2 || args[0].type != VM_STR || args[1].type != VM_STR) { vm_runtime_error("indexOf(str, search) expected\n"); }
            char *found = strstr(args[0].str, args[1].str);
            if (!found) return vm_num(-1);
            return vm_num(found - args[0].str);
        }
        case 19: { // replace
            if (argc != 3 || args[0].type != VM_STR || args[1].type != VM_STR || args[2].type != VM_STR) { vm_runtime_error("replace(str, search, replacement) expected\n"); }
            const char *search = args[1].str; int search_len = (int)strlen(search);
            if (search_len == 0) { char *c = malloc(strlen(args[0].str) + 1); strcpy(c, args[0].str); return vm_str(c); }
            int cap = (int)strlen(args[0].str) * 2 + 16; char *buf = malloc(cap); int blen = 0;
            const char *cur = args[0].str; const char *found; int rep_len = (int)strlen(args[2].str);
            while ((found = strstr(cur, search)) != NULL) {
                int prefix = (int)(found - cur);
                while (blen + prefix + rep_len + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
                memcpy(buf + blen, cur, prefix); blen += prefix;
                memcpy(buf + blen, args[2].str, rep_len); blen += rep_len;
                cur = found + search_len;
            }
            int remaining = (int)strlen(cur);
            while (blen + remaining + 1 > cap) { cap *= 2; buf = realloc(buf, cap); }
            memcpy(buf + blen, cur, remaining); blen += remaining; buf[blen] = '\0';
            return vm_str(buf);
        }
        case 20: { // upper
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("upper() expects a string\n"); }
            char *copy = malloc(strlen(args[0].str) + 1); strcpy(copy, args[0].str);
            for (char *p = copy; *p; p++) *p = (char)toupper((unsigned char)*p);
            return vm_str(copy);
        }
        case 21: { // lower
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("lower() expects a string\n"); }
            char *copy = malloc(strlen(args[0].str) + 1); strcpy(copy, args[0].str);
            for (char *p = copy; *p; p++) *p = (char)tolower((unsigned char)*p);
            return vm_str(copy);
        }
        case 22: { // trim
            if (argc != 1 || args[0].type != VM_STR) { vm_runtime_error("trim() expects a string\n"); }
            const char *s = args[0].str;
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            int slen = (int)strlen(s);
            while (slen > 0 && (s[slen - 1] == ' ' || s[slen - 1] == '\t' || s[slen - 1] == '\n' || s[slen - 1] == '\r')) slen--;
            char *buf = malloc(slen + 1); memcpy(buf, s, slen); buf[slen] = '\0';
            return vm_str(buf);
        }
    }
    vm_runtime_error("unknown builtin id: %d\n", id);
}
