local nums = [10, 20, 30];
each (x in nums) {
    say x;
}

// nested each loops
local a = [1, 2];
local b = [10, 20];
each (i in a) {
    each (j in b) {
        say i * j;
    }
}

// each loop calling a higher-order builtin in its body
forge double(x) { yield x * 2; }
each (x in nums) {
    local d = map([x], double);
    say d[0];
}

// empty array
local empty = [];
each (x in empty) {
    say "should not print";
}
say "after empty loop";

// each loop inside a function, recursion-safe check
forge sum_array(arr) {
    local total = 0;
    each (v in arr) {
        total = total + v;
    }
    yield total;
}
say sum_array(nums);
say sum_array([1, 2, 3, 4, 5]);

// loop variable reused/reassignable, doesn't affect iteration
each (x in nums) {
    x = x + 1000;
    say x;
}
