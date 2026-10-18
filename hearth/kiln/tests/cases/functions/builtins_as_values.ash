say type(len);
say map([1, 22, 333], str);
say map(["a", "bb"], len);
say map(["ab", "cd"], upper);
say filter([1, 2, 3, 4], forge (x) { yield x > 2; });
local f = len;
say f("hello");
say reduce([1, 2, 3], forge (a, b) { yield a + b; }, 0);
say map([4, 9], sqrt);
