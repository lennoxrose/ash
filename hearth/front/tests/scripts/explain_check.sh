#!/usr/bin/env bash
# Error codes: every code the compiler can report is documented in docs/MEMORY_SAFETY.md ("Error codes") and
# vice versa, the compiled-in copy (explain_data.c) is current, `ashvm explain` and `kiln --explain` print every
# entry, and every code appears in the expected output of some test (so each rule is exercised).
set -u
cd "$(dirname "$0")/../../../.."   # repository root
fail=0
if ! tools/gen_explain.sh | cmp -s - hearth/front/src/jexplain/C/explain_data.c; then
    echo "FAIL explain_data.c is out of date: tools/gen_explain.sh > hearth/front/src/jexplain/C/explain_data.c"; fail=1
fi
used=$(grep -rhoE '"E0[0-9]{3}"' hearth/front/src --include=*.c --exclude=explain_data.c | tr -d '"' | sort -u)
documented=$(grep -oE '^### E0[0-9]{3}' docs/MEMORY_SAFETY.md | cut -c5- | sort -u)
if [ "$used" != "$documented" ]; then echo "FAIL codes in the compiler and in the docs differ"; diff <(echo "$used") <(echo "$documented"); fail=1; fi
for c in $documented; do
    out=$(bin/ashvm explain "$c") || { echo "FAIL ashvm explain $c"; fail=1; continue; }
    [ -n "$out" ] && [ "$(bin/kiln --explain "$c")" == "$out" ] || { echo "FAIL kiln --explain $c differs"; fail=1; }
    if ! grep -rqF "[$c]" hearth/ashvm/tests/cases hearth/front/tests/cases --include=*.out --include=*.diag; then echo "FAIL no test prints $c"; fail=1; fi
done
[ $fail -eq 0 ] && echo "explain check ok ($(echo "$documented" | wc -l) codes)"
exit $fail
