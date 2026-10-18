local m = {};
m["b"] = 1; m["zeta"] = 2; m["alpha"] = 3; m["a"] = 4; m["mid"] = 5;
say keys(m);
say values(m);
delete(m, "alpha");
say keys(m);
m["alpha"] = 9;
m["b"] = 100;
say keys(m);
say m;
delete(m, "b");
delete(m, "zzz");
say keys(m);
say len(m);
say has(m, "b");
say has(m, "mid");
local big = {};
local i = 0;
during i < 100 { big[str(i)] = i; i++; }
i = 0;
during i < 100 { given i % 2 == 0 { delete(big, str(i)); } i++; }
say len(big);
say big["51"];
say keys(big)[0];
say keys(big)[49];
i = 0;
during i < 100 { given i % 2 == 0 { big[str(i)] = i; } i++; }
say len(big);
say keys(big)[50];
say big["98"];
