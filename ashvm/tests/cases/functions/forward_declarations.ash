say is_even(10);
say is_odd(7);
say is_even(7);
forge is_even(n) {
    given n == 0 { yield yes; }
    yield is_odd(n - 1);
}
forge is_odd(n) {
    given n == 0 { yield no; }
    yield is_even(n - 1);
}
say twice(3);
forge twice(x) { yield helper(x) * 2; }
forge helper(x) { yield x + 1; }
local f = helper;
say f(5);
say map([1, 2], twice);
forge shadow(a) { yield a; }
local shadow = forge (a) { yield a * 100; };
say shadow(2);
