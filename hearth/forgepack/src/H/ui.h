#ifndef FORGEPACK_UI_H
#define FORGEPACK_UI_H

// Plain Unicode symbols (box-drawing/dingbats block, not emoji -- no
// color-font fallback needed, renders in any monospace terminal font) and
// ANSI color, auto-disabled when stdout isn't a TTY or NO_COLOR is set
// (piping to a file or another program should never see escape codes).
void ui_ok(const char *fmt, ...);      // green check -- succeeded
void ui_err(const char *fmt, ...);     // red cross, to stderr -- failed
void ui_info(const char *fmt, ...);    // cyan arrow -- in progress / notice
void ui_bullet(const char *fmt, ...);  // dim bullet -- a sub-item (dependency list, etc.)
void ui_header(const char *fmt, ...);  // bold -- a section/project name header

#endif
