let m = {"a": 1, "b": 2};
print m["a"];
print m["b"];

m["c"] = 3;
print m["c"];

m["a"] = 99;
print m["a"];

print has(m, "b");
print has(m, "z");

delete(m, "b");
print has(m, "b");
print m["c"];

let g = {};
g["a"] = 1;
g["b"] = 2;
g["c"] = 3;
g["d"] = 4;
g["e"] = 5;
g["f"] = 6;
g["g"] = 7;
g["h"] = 8;
g["i"] = 9;
g["j"] = 10;
print g["a"];
print g["e"];
print g["j"];
