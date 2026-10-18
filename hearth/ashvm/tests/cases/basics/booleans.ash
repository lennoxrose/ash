say yes;
say no;
say 1 < 2;
say 2 < 1;
say not yes;
say not 0;
say 1 == 1;
say "a" == "a";
say "a" == "b";
say 1 and 1;
say 0 or 1;
say yes == 1;
say yes == yes;
say type(yes);
say type(1 < 2);
say [yes, no, 3 > 4];
say {"ok": yes};
say str(no) + str(yes);
say has({"a": 1}, "a");
say has({"a": 1}, "b");
say yes + 1;
say file_exists("/nonexistent");
say contains("abc", "b");
local flag = 5 > 3;
given flag { say "flag set"; }
given not flag { say "bad"; } otherwise { say "ok"; }
say flag and not no;
say none == none;
say not none;
attempt { given {"a": 1} { say "bad"; } } handle (e) { say "map condition"; }
attempt { given "x" { say "bad"; } } handle (e) { say "string condition"; }
local n = 0;
during n < 3 { n++; }
say n;
say yes ? "t" : "f";
