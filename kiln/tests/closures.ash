// M8: lambda assigned to a variable and called
let square = fn(x) { return x * x; };
print square(5);

// named function referenced as a value and called indirectly
fn add(a, b) { return a + b; }
let op = add;
print op(3, 4);

// genuine capture: adder-generator (partial application / currying)
fn make_adder(n) {
    return fn(x) { return x + n; };
}
let add5 = make_adder(5);
let add10 = make_adder(10);
print add5(1);
print add10(1);
print add5(2);

// higher-order function: pass a closure into a plain function
fn apply_twice(f, x) {
    return f(f(x));
}
print apply_twice(square, 3);

// closure capturing multiple outer variables
fn make_scaler(mul, offset) {
    return fn(x) { return x * mul + offset; };
}
let scale = make_scaler(2, 1);
print scale(10);
