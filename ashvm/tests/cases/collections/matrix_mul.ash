// matrix_mul(a, b) -- dispatches to Pyre's CPU backend for large square
// matrices (ash_gpu_matrix_multiply), computes directly in C for small
// ones (see src/builtins/C/matrix.c's ASH_GPU_MIN_N threshold). Both
// paths must agree.
local a = [[1, 2], [3, 4]];
local b = [[5, 6], [7, 8]];
say matrix_mul(a, b);

forge make_matrix(n, v) {
    local mat = [];
    local i = 0;
    during i < n {
        local row = [];
        local j = 0;
        during j < n {
            push(row, v);
            j += 1;
        }
        push(mat, row);
        i += 1;
    }
    yield mat;
}

// Above the dispatch threshold -- exercises the Pyre CPU backend.
local n = 200;
local big_a = make_matrix(n, 1.5);
local big_b = make_matrix(n, 2.0);
local big_c = matrix_mul(big_a, big_b);
say big_c[0][0];
say big_c[n - 1][n - 1];

// Error cases: not square, mismatched sizes, not arrays at all.
attempt { matrix_mul([1, 2, 3], [4, 5, 6]); } handle (e) { say "not nested: " + e; }
attempt { matrix_mul([[1, 2]], [[1, 2], [3, 4]]); } handle (e) { say "mismatched size: " + e; }
attempt { matrix_mul(5, 6); } handle (e) { say "not arrays: " + e; }
