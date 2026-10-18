forge make_adder(x) {
    yield forge(y) { yield x + y; };
}

local add5 = make_adder(5);
local add10 = make_adder(10);
say add5(3);
say add10(3);

local nums = [1, 2, 3];
local shift100 = make_adder(100);
local shifted = map(nums, shift100);
say shifted;

forge compose(f, g) {
    yield forge(x) { yield f(g(x)); };
}
forge double(x) { yield x * 2; }
forge inc(x) { yield x + 1; }
local doubleThenInc = compose(inc, double);
say doubleThenInc(5);
