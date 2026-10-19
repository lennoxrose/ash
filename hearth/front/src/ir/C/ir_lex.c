#include <string.h>
#include <ctype.h>
#include "ir/H/ir_lex.h"

/* lex_* names: the compilers keep their own `Token current` globals */
static const char *lex_start;
static const char *lex_current;
static int lex_line;
static const char *lex_source;

void lexer_init(const char *source) {
    lex_source = source;
    lex_start = source;
    lex_current = source;
    lex_line = 1;
}

LexerState lexer_save_state(void) {
    LexerState state = { lex_start, lex_current, lex_line };
    return state;
}

void lexer_restore_state(LexerState state) {
    lex_start = state.start;
    lex_current = state.current;
    lex_line = state.line;
}

static int is_at_end(void) { return *lex_current == '\0'; }
static char advance(void) { lex_current++; return lex_current[-1]; }
static char peek(void) { return *lex_current; }
static char peek_next(void) { if (is_at_end()) return '\0'; return lex_current[1]; }

static int match(char expected) {
    if (is_at_end()) return 0;
    if (*lex_current != expected) return 0;
    lex_current++;
    return 1;
}

static void skip_whitespace(void) {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ': case '\r': case '\t':
                advance();
                break;
            case '\n':
                lex_line++;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) advance();
                } else if (peek_next() == '*') {
                    advance(); advance();
                    while (!is_at_end() && !(peek() == '*' && peek_next() == '/')) {
                        if (peek() == '\n') lex_line++;
                        advance();
                    }
                    if (!is_at_end()) { advance(); advance(); }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = lex_start;
    token.length = (int)(lex_current - lex_start);
    token.line = lex_line;
    return token;
}

static Token error_token(const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lex_line;
    return token;
}

/* Only decides the token's boundaries; the compilers convert it with strtod,
 * which understands hex ("0x1A") and exponents ("1e10", "1.5e-3"). */
static Token number(void) {
    if (lex_start[0] == '0' && (peek() == 'x' || peek() == 'X')) {
        advance();
        while (isxdigit(peek())) advance();
        return make_token(TOKEN_NUMBER);
    }
    while (isdigit(peek())) advance();
    if (peek() == '.' && isdigit(peek_next())) {
        advance();
        while (isdigit(peek())) advance();
    }
    if (peek() == 'e' || peek() == 'E') {
        char after = peek_next();
        if (isdigit(after) || after == '+' || after == '-') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            while (isdigit(peek())) advance();
        }
    }
    return make_token(TOKEN_NUMBER);
}

static Token string_token(void) {
    const char *content_start = lex_current;
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\\' && peek_next() != '\0') {
            advance();
            advance();
            continue;
        }
        if (peek() == '\n') lex_line++;
        advance();
    }
    if (is_at_end()) return error_token("unterminated string");

    int length = (int)(lex_current - content_start);
    advance();

    Token token;
    token.type = TOKEN_STRING;
    token.start = content_start;
    token.length = length;
    token.line = lex_line;
    return token;
}

static const struct { const char *word; TokenType type; } keywords[] = {
    { "say", TOKEN_SAY }, { "local", TOKEN_LOCAL }, { "given", TOKEN_GIVEN },
    { "otherwise", TOKEN_OTHERWISE }, { "during", TOKEN_DURING }, { "each", TOKEN_EACH },
    { "in", TOKEN_IN }, { "forge", TOKEN_FORGE }, { "yield", TOKEN_YIELD },
    { "attempt", TOKEN_ATTEMPT }, { "handle", TOKEN_HANDLE }, { "raise", TOKEN_RAISE },
    { "stop", TOKEN_STOP }, { "next", TOKEN_NEXT }, { "yes", TOKEN_YES },
    { "no", TOKEN_NO }, { "none", TOKEN_NONE }, { "and", TOKEN_AND },
    { "or", TOKEN_OR }, { "not", TOKEN_BANG },
};

static Token identifier(void) {
    while (isalnum(peek()) || peek() == '_') advance();

    int length = (int)(lex_current - lex_start);
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if ((int)strlen(keywords[i].word) == length && strncmp(lex_start, keywords[i].word, (size_t)length) == 0)
            return make_token(keywords[i].type);
    }
    return make_token(TOKEN_IDENTIFIER);
}

Token lexer_next_token(void) {
    skip_whitespace();
    lex_start = lex_current;

    if (is_at_end()) return make_token(TOKEN_EOF);

    char c = advance();

    if (isdigit(c)) return number();
    if (isalpha(c) || c == '_') return identifier();
    if (c == '"') return string_token();

    switch (c) {
        case ';': return make_token(TOKEN_SEMICOLON);
        case ',': return make_token(TOKEN_COMMA);
        case ':': return make_token(TOKEN_COLON);
        case '?': return make_token(TOKEN_QUESTION);
        case '+':
            if (match('+')) return make_token(TOKEN_PLUS_PLUS);
            return make_token(match('=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
        case '-':
            if (match('-')) return make_token(TOKEN_MINUS_MINUS);
            return make_token(match('=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
        case '*':
            if (match('*')) return make_token(TOKEN_STAR_STAR);
            return make_token(match('=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
        case '/': return make_token(match('=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
        case '%': return make_token(TOKEN_PERCENT);
        case '(': return make_token(TOKEN_LPAREN);
        case ')': return make_token(TOKEN_RPAREN);
        case '{': return make_token(TOKEN_LBRACE);
        case '}': return make_token(TOKEN_RBRACE);
        case '[': return make_token(TOKEN_LBRACKET);
        case ']': return make_token(TOKEN_RBRACKET);
        case '=': return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '!':
            if (match('=')) return make_token(TOKEN_BANG_EQUAL);
            return error_token("unexpected character");
        case '<':
            if (match('<')) return make_token(TOKEN_SHL);
            return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            if (match('>')) return make_token(TOKEN_SHR);
            return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '&': return make_token(TOKEN_AMP);
        case '|': return make_token(TOKEN_PIPE);
        case '^': return make_token(TOKEN_CARET);
        case '~': return make_token(TOKEN_TILDE);
        case '.': return make_token(TOKEN_DOT);
    }

    return error_token("unexpected character");
}

/* The marker comment `//@<line>[ <name>]` on the line right before the token at `tok` (only white space between
 * them), as the lowering writes it before the statements of project functions; see jlower/C/lower_stmt.c. */
int ir_marker_before(const char *tok, int *line, const char **name, int *name_len) {
    const char *p = tok;
    while (p > lex_source && (p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n' || p[-1] == '\r')) p--;
    const char *ls = p;
    while (ls > lex_source && ls[-1] != '\n') ls--;
    while (ls < p && (*ls == ' ' || *ls == '\t')) ls++;
    if (p - ls < 4 || ls[0] != '/' || ls[1] != '/' || ls[2] != '@') return 0;
    const char *q = ls + 3;
    int v = 0, digits = 0;
    while (q < p && *q >= '0' && *q <= '9') { v = v * 10 + (*q - '0'); q++; digits++; }
    if (!digits) return 0;
    *line = v;
    *name = NULL;
    *name_len = 0;
    if (q < p && *q == ' ') { *name = q + 1; *name_len = (int)(p - (q + 1)); }
    return 1;
}
