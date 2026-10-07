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
