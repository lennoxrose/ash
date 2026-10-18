#include "jlex/H/jlex.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "jcommon/H/jcommon.h"

typedef struct {
    const char *word;
    JTokKind kind;
} Keyword;

static const Keyword keywords[] = {
    {"package", JT_PACKAGE}, {"import", JT_IMPORT}, {"class", JT_CLASS},
    {"interface", JT_INTERFACE}, {"enum", JT_ENUM}, {"extends", JT_EXTENDS},
    {"implements", JT_IMPLEMENTS}, {"abstract", JT_ABSTRACT}, {"new", JT_NEW},
    {"this", JT_THIS}, {"super", JT_SUPER}, {"public", JT_PUBLIC},
    {"private", JT_PRIVATE}, {"protected", JT_PROTECTED}, {"static", JT_STATIC},
    {"final", JT_FINAL}, {"const", JT_CONST}, {"native", JT_NATIVE}, {"if", JT_IF},
    {"else", JT_ELSE}, {"switch", JT_SWITCH}, {"case", JT_CASE},
    {"default", JT_DEFAULT}, {"while", JT_WHILE}, {"for", JT_FOR}, {"do", JT_DO},
    {"break", JT_BREAK}, {"continue", JT_CONTINUE}, {"return", JT_RETURN},
    {"try", JT_TRY}, {"catch", JT_CATCH}, {"finally", JT_FINALLY},
    {"throw", JT_THROW}, {"throws", JT_THROWS}, {"instanceof", JT_INSTANCEOF},
    {"void", JT_VOID}, {"true", JT_TRUE}, {"false", JT_FALSE}, {"null", JT_NULL},
    {"int", JT_INT}, {"long", JT_LONG}, {"float", JT_FLOAT}, {"double", JT_DOUBLE},
    {"bool", JT_BOOL}, {"char", JT_CHAR}, {"byte", JT_BYTE}, {"short", JT_SHORT},
};

static const char *tok_names[JT_COUNT] = {
    [JT_EOF] = "end of file", [JT_IDENT] = "identifier", [JT_INT_LIT] = "integer literal",
    [JT_LONG_LIT] = "long literal", [JT_FLOAT_LIT] = "float literal",
    [JT_DOUBLE_LIT] = "double literal", [JT_CHAR_LIT] = "char literal",
    [JT_STRING_LIT] = "string literal", [JT_LPAREN] = "'('", [JT_RPAREN] = "')'",
    [JT_LBRACE] = "'{'", [JT_RBRACE] = "'}'", [JT_LBRACKET] = "'['",
    [JT_RBRACKET] = "']'", [JT_SEMI] = "';'", [JT_COMMA] = "','", [JT_DOT] = "'.'",
    [JT_COLON] = "':'", [JT_ASSIGN] = "'='", [JT_LT] = "'<'", [JT_GT] = "'>'",
    [JT_ARROW] = "'->'", [JT_AT] = "'@'",
};

const char *jtok_name(JTokKind k) {
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (keywords[i].kind == k) return j_fmt("'%s'", keywords[i].word);
    }
    return tok_names[k] ? tok_names[k] : "token";
}

typedef struct {
    const char *path;
    const char *src;
    const char *p;
    int line;
    const char *line_start;
    JList list;
} Lexer;

static int cur_col(Lexer *lx, const char *at) { return (int)(at - lx->line_start) + 1; }

static void push(Lexer *lx, JTokKind kind, const char *text, int line, int col, int adj) {
    JToken *t = j_alloc(sizeof(JToken));
    t->kind = kind;
    t->text = text;
    t->line = line;
    t->col = col;
    t->adj = adj;
    j_list_push(&lx->list, t);
}

/* Reads one (possibly escaped) character from a char/string literal into *out. */
static int read_escape(Lexer *lx, int *out) {
    char c = *lx->p++;
    switch (c) {
        case 'n': *out = '\n'; return 1;
        case 't': *out = '\t'; return 1;
        case 'r': *out = '\r'; return 1;
        case '0': *out = 0; return 1;
        case '\\': *out = '\\'; return 1;
        case '"': *out = '"'; return 1;
        case '\'': *out = '\''; return 1;
        case 'x': {
            int v = 0;
            for (int i = 0; i < 2; i++) {
                char h = *lx->p;
                if (!isxdigit((unsigned char)h)) break;
                v = v * 16 + (isdigit((unsigned char)h) ? h - '0' : (tolower(h) - 'a' + 10));
                lx->p++;
            }
            *out = v;
            return 1;
        }
        default:
            j_error(lx->path, lx->line, cur_col(lx, lx->p - 1), "unknown escape sequence '\\%c'", c);
    }
    return 0;
}

JTokens jlex_tokenize(const char *path, const char *src) {
    Lexer lx = {path, src, src, 1, src, {0}};
    int prev_end_line = -1;
    const char *prev_end = NULL;
    for (;;) {
        /* whitespace and comments */
        for (;;) {
            char c = *lx.p;
            if (c == '\n') {
                lx.p++;
                lx.line++;
                lx.line_start = lx.p;
            } else if (c == ' ' || c == '\t' || c == '\r') {
                lx.p++;
            } else if (c == '/' && lx.p[1] == '/') {
                while (*lx.p && *lx.p != '\n') lx.p++;
            } else if (c == '/' && lx.p[1] == '*') {
                int sl = lx.line;
                const char *sp = lx.p;
                int sc = cur_col(&lx, sp);
                lx.p += 2;
                while (*lx.p && !(lx.p[0] == '*' && lx.p[1] == '/')) {
                    if (*lx.p == '\n') {
                        lx.line++;
                        lx.line_start = lx.p + 1;
                    }
                    lx.p++;
                }
                if (!*lx.p) j_error(path, sl, sc, "unterminated block comment");
                lx.p += 2;
            } else {
                break;
            }
        }
        int line = lx.line;
        int col = cur_col(&lx, lx.p);
        int adj = prev_end && lx.p == prev_end && prev_end_line == line;
        char c = *lx.p;
        if (!c) {
            push(&lx, JT_EOF, "", line, col, 0);
            break;
        }
        const char *start = lx.p;
        if (isalpha((unsigned char)c) || c == '_' || c == '$') {
            while (isalnum((unsigned char)*lx.p) || *lx.p == '_' || *lx.p == '$') lx.p++;
            size_t n = (size_t)(lx.p - start);
            JTokKind kind = JT_IDENT;
            for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
                if (strlen(keywords[i].word) == n && strncmp(keywords[i].word, start, n) == 0) {
                    kind = keywords[i].kind;
                    break;
                }
            }
            push(&lx, kind, j_strndup(start, n), line, col, adj);
        } else if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)lx.p[1]))) {
            JTokKind kind = JT_INT_LIT;
            if (c == '0' && (lx.p[1] == 'x' || lx.p[1] == 'X')) {
                lx.p += 2;
                while (isxdigit((unsigned char)*lx.p) || *lx.p == '_') lx.p++;
            } else {
                while (isdigit((unsigned char)*lx.p) || *lx.p == '_') lx.p++;
                if (*lx.p == '.' && isdigit((unsigned char)lx.p[1])) {
                    kind = JT_DOUBLE_LIT;
                    lx.p++;
                    while (isdigit((unsigned char)*lx.p) || *lx.p == '_') lx.p++;
                } else if (*lx.p == '.' && !isalpha((unsigned char)lx.p[1]) && lx.p[1] != '.') {
                    kind = JT_DOUBLE_LIT;
                    lx.p++;
                }
                if (*lx.p == 'e' || *lx.p == 'E') {
                    const char *q = lx.p + 1;
                    if (*q == '+' || *q == '-') q++;
                    if (isdigit((unsigned char)*q)) {
                        kind = JT_DOUBLE_LIT;
                        lx.p = q;
                        while (isdigit((unsigned char)*lx.p)) lx.p++;
                    }
                }
            }
            size_t n = (size_t)(lx.p - start);
            char *text = j_alloc(n + 1);
            size_t k = 0;
            for (size_t i = 0; i < n; i++) {
                if (start[i] != '_') text[k++] = start[i];
            }
            if (*lx.p == 'L' || *lx.p == 'l') {
                if (kind != JT_INT_LIT) j_error(path, line, col, "invalid long literal");
                kind = JT_LONG_LIT;
                lx.p++;
            } else if (*lx.p == 'f' || *lx.p == 'F') {
                kind = JT_FLOAT_LIT;
                lx.p++;
            } else if (*lx.p == 'd' || *lx.p == 'D') {
                kind = JT_DOUBLE_LIT;
                lx.p++;
            }
            push(&lx, kind, text, line, col, adj);
        } else if (c == '"') {
            lx.p++;
            JBuf b = {0};
            while (*lx.p != '"') {
                if (!*lx.p || *lx.p == '\n') j_error(path, line, col, "unterminated string literal");
                if (*lx.p == '\\') {
                    lx.p++;
                    int v;
                    read_escape(&lx, &v);
                    j_buf_addc(&b, (char)v);
                } else {
                    j_buf_addc(&b, *lx.p++);
                }
            }
            lx.p++;
            push(&lx, JT_STRING_LIT, b.data ? b.data : "", line, col, adj);
        } else if (c == '\'') {
            lx.p++;
            int v;
            if (*lx.p == '\\') {
                lx.p++;
                read_escape(&lx, &v);
            } else {
                v = (unsigned char)*lx.p++;
            }
            if (*lx.p != '\'') j_error(path, line, col, "unterminated char literal");
            lx.p++;
            push(&lx, JT_CHAR_LIT, j_fmt("%d", v), line, col, adj);
        } else {
            lx.p++;
            char n1 = *lx.p;
            char n2 = n1 ? lx.p[1] : 0;
            JTokKind k = JT_EOF;
#define T1(kind) do { k = kind; } while (0)
#define T2(kind) do { k = kind; lx.p += 1; } while (0)
#define T3(kind) do { k = kind; lx.p += 2; } while (0)
            switch (c) {
                case '(': T1(JT_LPAREN); break;
                case ')': T1(JT_RPAREN); break;
                case '{': T1(JT_LBRACE); break;
                case '}': T1(JT_RBRACE); break;
                case '[': T1(JT_LBRACKET); break;
                case ']': T1(JT_RBRACKET); break;
                case ';': T1(JT_SEMI); break;
                case ',': T1(JT_COMMA); break;
                case '@': T1(JT_AT); break;
                case '~': T1(JT_TILDE); break;
                case '?': T1(JT_QUESTION); break;
                case '.':
                    if (n1 == '.' && n2 == '.') T3(JT_ELLIPSIS); else T1(JT_DOT);
                    break;
                case ':':
                    if (n1 == ':') T2(JT_COLONCOLON); else T1(JT_COLON);
                    break;
                case '=':
                    if (n1 == '=') T2(JT_EQ); else T1(JT_ASSIGN);
                    break;
                case '!':
                    if (n1 == '=') T2(JT_NE); else T1(JT_BANG);
                    break;
                case '+':
                    if (n1 == '+') T2(JT_INC); else if (n1 == '=') T2(JT_PLUS_ASSIGN); else T1(JT_PLUS);
                    break;
                case '-':
                    if (n1 == '-') T2(JT_DEC); else if (n1 == '=') T2(JT_MINUS_ASSIGN);
                    else if (n1 == '>') T2(JT_ARROW); else T1(JT_MINUS);
                    break;
                case '*':
                    if (n1 == '=') T2(JT_STAR_ASSIGN); else T1(JT_STAR);
                    break;
                case '/':
                    if (n1 == '=') T2(JT_SLASH_ASSIGN); else T1(JT_SLASH);
                    break;
                case '%':
                    if (n1 == '=') T2(JT_PERCENT_ASSIGN); else T1(JT_PERCENT);
                    break;
                case '&':
                    if (n1 == '&') T2(JT_ANDAND); else if (n1 == '=') T2(JT_AMP_ASSIGN); else T1(JT_AMP);
                    break;
                case '|':
                    if (n1 == '|') T2(JT_OROR); else if (n1 == '=') T2(JT_PIPE_ASSIGN); else T1(JT_PIPE);
                    break;
                case '^':
                    if (n1 == '=') T2(JT_CARET_ASSIGN); else T1(JT_CARET);
                    break;
                case '<':
                    if (n1 == '<' && n2 == '=') T3(JT_SHL_ASSIGN);
                    else if (n1 == '<') T2(JT_SHL);
                    else if (n1 == '=') T2(JT_LE);
                    else T1(JT_LT);
                    break;
                case '>':
                    if (n1 == '=') T2(JT_GE); else T1(JT_GT);
                    break;
                default:
                    j_error(path, line, col, "unexpected character '%c'", c);
            }
#undef T1
#undef T2
#undef T3
            push(&lx, k, j_strndup(start, (size_t)(lx.p - start)), line, col, adj);
        }
        prev_end = lx.p;
        prev_end_line = lx.line;
    }
    JTokens out;
    out.path = path;
    out.count = lx.list.count;
    out.toks = j_alloc(sizeof(JToken) * (size_t)out.count);
    for (int i = 0; i < out.count; i++) out.toks[i] = *(JToken *)lx.list.items[i];
    return out;
}
