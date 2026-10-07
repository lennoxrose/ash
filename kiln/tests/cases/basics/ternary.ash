say yes ? "yes" : "no";
say no ? "yes" : "no";
local x = 5;
say x > 3 ? "big" : "small";
say x < 3 ? "big" : x < 10 ? "medium" : "huge";
local m = { "a": yes, "b": no };
say m["a"] ? "a is true" : "a is false";
forge max(a, b) { yield a > b ? a : b; }
say max(3, 7);
say max(10, 2);
