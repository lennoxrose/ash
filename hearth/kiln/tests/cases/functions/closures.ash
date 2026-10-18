// M8: lambda assigned to a variable and called
local square = forge(x) { yield x * x; };
say square(5);

// named function referenced as a value and called indirectly
forge add(a, b) { yield a + b; }
local op = add;
say op(3, 4);

// genuine capture: adder-generator (partial application / currying)
forge make_adder(n) {
    yield forge(x) { yield x + n; };
}
local add5 = make_adder(5);
local add10 = make_adder(10);
say add5(1);
say add10(1);
say add5(2);

// higher-order function: pass a closure into a plain function
forge apply_twice(f, x) {
    yield f(f(x));
}
say apply_twice(square, 3);

// closure capturing multiple outer variables
forge make_scaler(mul, offset) {
    yield forge(x) { yield x * mul + offset; };
}
local scale = make_scaler(2, 1);
say scale(10);
