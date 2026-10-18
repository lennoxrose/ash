local i = 0;
during (i < 10) {
    i = i + 1;
    given (i == 5) { stop; }
    say i;
}
say "---";

local j = 0;
during (j < 5) {
    j = j + 1;
    given (j == 3) { next; }
    say j;
}
say "---";

each (x in [1,2,3,4,5]) {
    given (x == 3) { stop; }
    say x;
}
say "---";

each (x in [1,2,3,4,5]) {
    given (x == 3) { next; }
    say x;
}
say "---";

// nested loops: stop only exits innermost
each (i in [1,2]) {
    each (j in [1,2,3]) {
        given (j == 2) { stop; }
        say i * 10 + j;
    }
}
say "---";

// stop/next inside function's own loop, not affecting caller
forge find_first_even(arr) {
    each (x in arr) {
        given (x - floor(x/2)*2 == 0) {
            yield x;
        }
    }
    yield -1;
}
say find_first_even([1, 3, 5, 4, 7]);
