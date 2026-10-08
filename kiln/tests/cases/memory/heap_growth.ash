// Allocates well past the old fixed 16MB bump-heap cap (each iteration
// leaks a fresh 4-element array, nothing is ever freed) to exercise the
// allocator's dynamic growth instead of segfaulting partway through.
local total = 0;
local i = 0;
during (i < 2000000) {
    local arr = [i, i + 1, i + 2, i + 3];
    total = total + arr[0];
    i = i + 1;
}
say total;
