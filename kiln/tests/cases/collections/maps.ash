local m = {"a": 1, "b": 2};
say m["a"];
say m["b"];

m["c"] = 3;
say m["c"];

m["a"] = 99;
say m["a"];

say has(m, "b");
say has(m, "z");

delete(m, "b");
say has(m, "b");
say m["c"];

local g = {};
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
say g["a"];
say g["e"];
say g["j"];
