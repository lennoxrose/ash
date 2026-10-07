#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOKEN_PRINT,
    TOKEN_LET,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_IMPORT,
    TOKEN_FN,
    TOKEN_RETURN,
    TOKEN_TRY,
    TOKEN_CATCH,
    TOKEN_THROW,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NIL,
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_IMPORT_PATH,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG_EQUAL,
    TOKEN_BANG,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_DOT,
    TOKEN_QUESTION,
    TOKEN_AMP,
    TOKEN_PIPE,
    TOKEN_CARET,
    TOKEN_TILDE,
    TOKEN_SHL,
    TOKEN_SHR,
    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR_EQUAL,
    TOKEN_SLASH_EQUAL,
    TOKEN_PLUS_PLUS,
    TOKEN_MINUS_MINUS,
    TOKEN_STAR_STAR,
    TOKEN_NUMBER,
    TOKEN_SEMICOLON,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    int length;
    int line;
} Token;

void lexer_init(const char *source);
Token lexer_next_token(void);

// Saved lexer cursor position -- used by imports.c to recursively lex a
// different source buffer for an imported file, then resume exactly where
// the importing file's tokenizer left off (mirrors vars_save/restore's
// save-a-struct, recurse, restore pattern).
typedef struct {
    const char *start;
    const char *current;
    int line;
} LexerState;

LexerState lexer_save_state(void);
void lexer_restore_state(LexerState state);

// Called by imports.c right after the parser has consumed a TOKEN_LESS
// that opened an `@import <...>` path. Scans raw characters (not through
// the normal keyword/operator rules -- a path like "./foo.ash" contains
// '.' and '/' which aren't otherwise valid outside this context) up to
// and INCLUDING the closing '>', and returns a token whose start/length
// cover just the path text (excluding both brackets). Errors (via the
// TOKEN_ERROR convention) on a newline or EOF before '>'.
Token lexer_scan_import_path(void);

#endif
