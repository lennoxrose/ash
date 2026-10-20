#!/usr/bin/env bash
# ashvm/tests/scripts/lsp_check.sh -- `ashvm lsp` against a scripted LSP client (lsp_check.py): initialize, diagnostics
# (type errors, ownership errors with their notes, errors in another file, UTF-16 / UTF-8 positions, clearing when the
# text is fixed), hover and go-to-definition for locals, parameters, fields, methods, classes (own file, other file,
# the library), and bad input of every kind (garbage bodies and headers, wrong types, deep nesting, a file the front end
# cannot finish) after which the server must still answer.
set -uo pipefail
cd "$(dirname "$0")/../.." # -> ashvm/
export ASH_PATH="${ASH_PATH:-$PWD/../../../ash-stdlib/src/main/ash}"
make -s || { echo "build failed"; exit 1; }
exec python3 "$PWD/tests/scripts/lsp_check.py" "$PWD/../../bin/ashvm"
