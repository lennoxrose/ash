fn double(x) { return x * 2; }
fn is_even(x) { return x % 2 == 0; }

let nums = [1, 2, 3, 4, 5, 6];

let doubled = map(nums, double);
print doubled;

let evens = filter(nums, is_even);
print evens;

let total = reduce(nums, fn(acc, x) { return acc + x; }, 0);
print total;

let squareLambda = fn(x) { return x * x; };
let squares = map(nums, squareLambda);
print squares;

let f = double;
print f(21);
