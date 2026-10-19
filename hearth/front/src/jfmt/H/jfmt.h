#ifndef JFMT_H
#define JFMT_H

/* `ashvm fmt`: whitespace hygiene for Ash source, nothing else.
 *
 * It re-indents lines by brace depth (4 spaces; a `case` body one level deeper), keeps the author's relative
 * indentation of continuation lines (a line that does not start a statement moves with the statement it belongs to),
 * turns tabs in the indentation into spaces, strips trailing white space, collapses runs of blank lines to one, and
 * ends the file with exactly one newline. It never changes anything inside a line: no spacing around operators, no
 * line breaking, comments and strings untouched. After formatting, the token stream of the result is compared with
 * the original's (the lexer of the compiler); if they differ the text is not returned. */

/* Returns the formatted text (arena string), or NULL with *err set to a message when the source cannot be formatted
 * (lexical error, nesting too deep, or a token mismatch, which would be a bug). */
char *jfmt_format(const char *path, const char *src, const char **err);

#endif
