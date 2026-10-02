#include "lexer/lexer_import.h"

LexerState lexer_save_state(void) {
    LexerState state;
    state.start = lex_start;
    state.current = lex_current;
    state.line = lex_line;
    return state;
}

void lexer_restore_state(LexerState state) {
    lex_start = state.start;
    lex_current = state.current;
    lex_line = state.line;
}

// Called with the lexer's internal `current` pointer sitting immediately
// after the '<' the parser just confirmed (via current.type==TOKEN_LESS)
// WITHOUT calling advance_token() on it -- so `current` here still points
// at the first path character, untouched by normal tokenization. Scans
// raw bytes up to '>', consuming the '>' too, so the parser's next
// advance_token() call correctly picks up whatever follows it (';').
Token lexer_scan_import_path(void) {
    const char *content_start = lex_current;
    while (peek() != '>' && peek() != '\n' && !is_at_end()) advance();
    if (peek() != '>') return error_token("unterminated import path (expected '>')");

    int length = (int)(lex_current - content_start);
    advance(); // consume '>'

    Token token;
    token.type = TOKEN_IMPORT_PATH;
    token.start = content_start;
    token.length = length;
    token.line = lex_line;
    return token;
}
