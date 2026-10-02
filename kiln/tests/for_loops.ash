let nums = [10, 20, 30];
for (x in nums) {
    print x;
}

// nested for loops
let a = [1, 2];
let b = [10, 20];
for (i in a) {
    for (j in b) {
        print i * j;
    }
}

// for loop calling a higher-order builtin in its body
fn double(x) { return x * 2; }
for (x in nums) {
    let d = map([x], double);
    print d[0];
}

// empty array
let empty = [];
for (x in empty) {
    print "should not print";
}
print "after empty loop";

// for loop inside a function, recursion-safe check
fn sum_array(arr) {
    let total = 0;
    for (v in arr) {
        total = total + v;
    }
    return total;
}
print sum_array(nums);
print sum_array([1, 2, 3, 4, 5]);

// loop variable reused/reassignable, doesn't affect iteration
for (x in nums) {
    x = x + 1000;
    print x;
}
