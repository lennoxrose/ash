// C[i][j] = val -- nested index mutation without a manual intermediate
// extraction (local row = C[i]; row[j] = val; C[i] = row;).
local a = [[1, 2], [3, 4]];
a[0][1] = 99;
say a;

// three levels deep
local b = [[[1, 2], [3, 4]], [[5, 6], [7, 8]]];
b[1][0][1] = 999;
say b;

// matrix-multiply-shaped accumulation: a 2D result built with plain `=`,
// not a manual row-then-store dance
local n = 3;
local c = [[0, 0, 0], [0, 0, 0], [0, 0, 0]];
local i = 0;
during (i < n) {
    local j = 0;
    during (j < n) {
        c[i][j] = (i * n) + j;
        j = j + 1;
    }
    i = i + 1;
}
say c;

// mixed array/map chain
local m = {"row": [1, 2, 3]};
m["row"][1] = 42;
say m["row"];

// plain single-level assignment still works (no regression)
local arr = [1, 2, 3];
arr[0] = 7;
say arr;

// a string anywhere in the chain is still immutable
attempt { local s = ["hi"]; s[0][0] = "x"; } handle (e) { say "immutable"; }
