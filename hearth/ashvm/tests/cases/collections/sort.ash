local a = [5, 2, 9, 1, 5, 6, -3, 0];
sort(a);
say a;
local s = ["pear", "apple", "fig", "banana", "apple", ""];
say sort(s);
local m = [{"k": 2, "t": "a"}, {"k": 1, "t": "b"}, {"k": 2, "t": "c"}, {"k": 1, "t": "d"}];
sort(m, forge (x, y) { yield x["k"] - y["k"]; });
say map(m, forge (x) { yield x["t"]; });
say sort([3, 1, 2], forge (x, y) { yield y - x; });
say sort([]);
say sort([1]);
local big = [];
local i = 0;
during i < 200 { push(big, (i * 37) % 101); i++; }
sort(big);
local ok = yes;
i = 1;
during i < len(big) { given big[i - 1] > big[i] { ok = no; } i++; }
say ok;
say len(big);
