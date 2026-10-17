#ifndef JLEX_H
#define JLEX_H

/* Lexer for new (Java-like) Ash. Tokenizes a whole file up front so the parser can
 * backtrack. `>` is always a single token (generics); the parser merges adjacent
 * `>` tokens into shift operators using the `adj` flag. Comments (//, ///, block)
 * are skipped. */

typedef enum {
    JT_EOF,
    JT_IDENT,
    JT_INT_LIT,    /* 42, 0x1F */
    JT_LONG_LIT,   /* 42L */
    JT_FLOAT_LIT,  /* 1.5f */
    JT_DOUBLE_LIT, /* 1.5, 1e3 */
    JT_CHAR_LIT,   /* 'a' (text holds the decoded code point as a number) */
    JT_STRING_LIT, /* decoded text */
    /* reserved words */
    JT_PACKAGE, JT_IMPORT, JT_CLASS, JT_INTERFACE, JT_ENUM, JT_EXTENDS, JT_IMPLEMENTS,
    JT_ABSTRACT, JT_NEW, JT_THIS, JT_SUPER, JT_PUBLIC, JT_PRIVATE, JT_PROTECTED,
    JT_STATIC, JT_FINAL, JT_CONST, JT_NATIVE, JT_IF, JT_ELSE, JT_SWITCH, JT_CASE,
    JT_DEFAULT, JT_WHILE, JT_FOR, JT_DO, JT_BREAK, JT_CONTINUE, JT_RETURN, JT_TRY,
    JT_CATCH, JT_FINALLY, JT_THROW, JT_THROWS, JT_INSTANCEOF, JT_VOID, JT_TRUE,
    JT_FALSE, JT_NULL, JT_INT, JT_LONG, JT_FLOAT, JT_DOUBLE, JT_BOOL, JT_CHAR,
    JT_BYTE, JT_SHORT,
    /* punctuation */
    JT_LPAREN, JT_RPAREN, JT_LBRACE, JT_RBRACE, JT_LBRACKET, JT_RBRACKET,
    JT_SEMI, JT_COMMA, JT_DOT, JT_ELLIPSIS, JT_AT, JT_COLONCOLON, JT_COLON,
    JT_QUESTION, JT_ARROW,
    /* operators */
    JT_ASSIGN, JT_PLUS_ASSIGN, JT_MINUS_ASSIGN, JT_STAR_ASSIGN, JT_SLASH_ASSIGN,
    JT_PERCENT_ASSIGN, JT_AMP_ASSIGN, JT_PIPE_ASSIGN, JT_CARET_ASSIGN, JT_SHL_ASSIGN,
    JT_EQ, JT_NE, JT_LT, JT_LE, JT_GT, JT_GE, JT_SHL,
    JT_PLUS, JT_MINUS, JT_STAR, JT_SLASH, JT_PERCENT, JT_AMP, JT_PIPE, JT_CARET,
    JT_TILDE, JT_BANG, JT_ANDAND, JT_OROR, JT_INC, JT_DEC,
    JT_COUNT
} JTokKind;

typedef struct {
    JTokKind kind;
    const char *text; /* identifier / literal text; decoded for strings */
    int line;
    int col;
    int adj; /* 1 when no whitespace separates this token from the previous one */
} JToken;

typedef struct {
    const char *path;
    JToken *toks;
    int count;
} JTokens;

JTokens jlex_tokenize(const char *path, const char *src);
const char *jtok_name(JTokKind k);

#endif
