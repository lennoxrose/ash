#ifndef KILN_WIN_FILE_CALL_H
#define KILN_WIN_FILE_CALL_H
#include "pe/H/pe_writer.h"

// Shared shape between ReadFile and WriteFile's argument lists (both are
// (handle, buffer, length, &ignored_out, NULL)) -- used by both
// codegen/C/platform/platform_console.c (stdin/stdout/stderr) and
// codegen/C/platform/platform_file.c (real files), so it lives here instead of
// being duplicated in each.
//
// In: RAX=handle, RSI=buf, RDI=len. `which` must be PE_IMPORT_READ_FILE
// or PE_IMPORT_WRITE_FILE.
void win_file_call(CodeBuf *code, PeImport which);

#endif
