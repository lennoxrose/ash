#ifndef JLSP_H
#define JLSP_H

/* `ashvm lsp`: a small Language Server Protocol server over stdio (JSON-RPC with Content-Length framing).
 *
 *   - textDocument/publishDiagnostics: the diagnostics of `ashvm check --json` for the open document (its text as the
 *     editor has it; the files it imports are read from disk);
 *   - textDocument/hover: the type of the local, field, method, class or expression under the cursor;
 *   - textDocument/definition: where that local, field, method or class is declared.
 *
 * Every analysis runs in a child process (fork): the front end keeps all its state in globals, ends compilations with
 * exit() and never frees, so a child gives each request a clean slate and no input can take the server down -- a child
 * that crashes, runs out of memory or exceeds its time limit costs one diagnostic ("the analysis failed"), not the
 * session. The text of a document is compiled in the project the file belongs to (the nearest directory above it
 * with an ash.pkg is the working directory of the child, which is where @ash-modules is looked for).
 *
 * Returns the process exit code: 0 after shutdown + exit, 1 when the client just went away. */
int jlsp_serve(void);

#endif
