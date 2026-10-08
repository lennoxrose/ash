#include <string.h>
#include <ctype.h>
#include "lexer/H/lexer.h"
#include "lexer/H/lexer_import.h"

// lex_* (not the bare start/current/line these had as file-static
// before) -- shared with lexer_import.c now, and a plain `current` would
// collide with compiler/H/state.h's own global `Token current` at link time.
const char *lex_start;
const char *lex_current;
int lex_line;

void lexer_init(const char *source) {
    lex_start = source;
    lex_current = source;
    lex_line = 1;
}

int is_at_end(void) { return *lex_current == '\0'; }
char advance(void) { lex_current++; return lex_current[-1]; }
char peek(void) { return *lex_current; }
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

Token error_token(const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lex_line;
    return token;
}

static Token number(void) {
    while (isdigit(peek())) advance();
    if (peek() == '.' && isdigit(peek_next())) {
        advance(); // consume '.'
        while (isdigit(peek())) advance();
    }
    if (peek() == 'e' || peek() == 'E') {
        char after = peek_next();
        if (isdigit(after) || after == '+' || after == '-') {
            advance(); // consume 'e'/'E'
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
            advance(); // skip the backslash
            advance(); // skip the escaped character itself, so a \" doesn't end the string early
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

static Token identifier(void) {
    while (isalnum(peek()) || peek() == '_') advance();

    int length = (int)(lex_current - lex_start);
    if (length == 3 && strncmp(lex_start, "say", 3) == 0) return make_token(TOKEN_SAY);
    if (length == 5 && strncmp(lex_start, "local", 5) == 0) return make_token(TOKEN_LOCAL);
    if (length == 5 && strncmp(lex_start, "given", 5) == 0) return make_token(TOKEN_GIVEN);
    if (length == 9 && strncmp(lex_start, "otherwise", 9) == 0) return make_token(TOKEN_OTHERWISE);
    if (length == 6 && strncmp(lex_start, "during", 6) == 0) return make_token(TOKEN_DURING);
    if (length == 4 && strncmp(lex_start, "each", 4) == 0) return make_token(TOKEN_EACH);
    if (length == 2 && strncmp(lex_start, "in", 2) == 0) return make_token(TOKEN_IN);
    if (length == 5 && strncmp(lex_start, "forge", 5) == 0) return make_token(TOKEN_FORGE);
    if (length == 5 && strncmp(lex_start, "yield", 5) == 0) return make_token(TOKEN_YIELD);
    if (length == 7 && strncmp(lex_start, "attempt", 7) == 0) return make_token(TOKEN_ATTEMPT);
    if (length == 6 && strncmp(lex_start, "handle", 6) == 0) return make_token(TOKEN_HANDLE);
    if (length == 5 && strncmp(lex_start, "raise", 5) == 0) return make_token(TOKEN_RAISE);
    if (length == 4 && strncmp(lex_start, "stop", 4) == 0) return make_token(TOKEN_STOP);
    if (length == 4 && strncmp(lex_start, "next", 4) == 0) return make_token(TOKEN_NEXT);
    if (length == 3 && strncmp(lex_start, "yes", 3) == 0) return make_token(TOKEN_YES);
    if (length == 2 && strncmp(lex_start, "no", 2) == 0) return make_token(TOKEN_NO);
    if (length == 4 && strncmp(lex_start, "none", 4) == 0) return make_token(TOKEN_NONE);
    if (length == 3 && strncmp(lex_start, "and", 3) == 0) return make_token(TOKEN_AND);
    if (length == 2 && strncmp(lex_start, "or", 2) == 0) return make_token(TOKEN_OR);
    // "not" reuses TOKEN_BANG (unary logical negation) -- same meaning as
    // the symbol it replaces, so unary()'s existing TOKEN_BANG handling in
    // expr.c needs no change at all.
    if (length == 3 && strncmp(lex_start, "not", 3) == 0) return make_token(TOKEN_BANG);

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
        case '.': return make_token(TOKEN_DOT);
        case '@':
            if (strncmp(lex_current, "import", 6) == 0 && !(isalnum((unsigned char)lex_current[6]) || lex_current[6] == '_')) {
                lex_current += 6;
                return make_token(TOKEN_IMPORT);
            }
            return error_token("expected 'import' after '@'");
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
        case '*': return make_token(match('=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
        case '/': return make_token(match('=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
        case '%': return make_token(TOKEN_PERCENT);
        case '&': return make_token(TOKEN_AMP);
        case '|': return make_token(TOKEN_PIPE);
        case '^': return make_token(TOKEN_CARET);
        case '~': return make_token(TOKEN_TILDE);
        case '(': return make_token(TOKEN_LPAREN);
        case ')': return make_token(TOKEN_RPAREN);
        case '{': return make_token(TOKEN_LBRACE);
        case '}': return make_token(TOKEN_RBRACE);
        case '[': return make_token(TOKEN_LBRACKET);
        case ']': return make_token(TOKEN_RBRACKET);
        case '=': return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        // Bare '!' is no longer valid on its own -- logical negation is the
        // 'not' keyword now (see identifier()). '!=' (inequality) stays
        // symbolic either way.
        case '!':
            if (match('=')) return make_token(TOKEN_BANG_EQUAL);
            return error_token("unexpected character");
        case '<':
            if (match('<')) return make_token(TOKEN_SHL);
            return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            if (match('>')) return make_token(TOKEN_SHR);
            return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    }

    return error_token("unexpected character");
}
