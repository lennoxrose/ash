fn make_adder(x) {
    return fn(y) { return x + y; };
}

let add5 = make_adder(5);
let add10 = make_adder(10);
print add5(3);
print add10(3);

let nums = [1, 2, 3];
let shift100 = make_adder(100);
let shifted = map(nums, shift100);
print shifted;

fn compose(f, g) {
    return fn(x) { return f(g(x)); };
}
fn double(x) { return x * 2; }
fn inc(x) { return x + 1; }
let doubleThenInc = compose(inc, double);
print doubleThenInc(5);

let total = reduce(nums, fn(acc, n) { return acc + n; }, 0);
print total;
