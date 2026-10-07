#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOKEN_SAY,
    TOKEN_LOCAL,
    TOKEN_GIVEN,
    TOKEN_OTHERWISE,
    TOKEN_DURING,
    TOKEN_EACH,
    TOKEN_IN,
    TOKEN_IMPORT,
    TOKEN_FORGE,
    TOKEN_YIELD,
    TOKEN_ATTEMPT,
    TOKEN_HANDLE,
    TOKEN_RAISE,
    TOKEN_STOP,
    TOKEN_NEXT,
    TOKEN_YES,
    TOKEN_NO,
    TOKEN_NONE,
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

typedef struct {
    const char *start;
    const char *current;
    int line;
} LexerState;

LexerState lexer_save_state(void);
void lexer_restore_state(LexerState state);
Token lexer_scan_import_path(void);

#endif
