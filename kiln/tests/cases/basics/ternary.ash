print true ? "yes" : "no";
print false ? "yes" : "no";
let x = 5;
print x > 3 ? "big" : "small";
print x < 3 ? "big" : x < 10 ? "medium" : "huge";
let m = { "a": true, "b": false };
print m["a"] ? "a is true" : "a is false";
fn max(a, b) { return a > b ? a : b; }
print max(3, 7);
print max(10, 2);
