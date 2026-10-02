let nums = [1, 2, 3, 4, 5];

fn double(x) { return x * 2; }
let doubled = map(nums, double);
print doubled[0];
print doubled[1];
print doubled[4];
print len(doubled);

fn is_even(x) { return x - floor(x / 2) * 2 == 0; }
let evens = filter(nums, is_even);
print len(evens);
print evens[0];
print evens[1];

fn add(a, b) { return a + b; }
print reduce(nums, add, 0);
print reduce(nums, add, 100);

let squares = map(nums, fn(x) { return x * x; });
print squares[2];
print squares[4];

let empty = [];
let mapped_empty = map(empty, double);
print len(mapped_empty);
