local a = [1, 2, 3];
attempt { delete(a, 0); say "deleted"; say a; push(a, 9); delete(a, 99); say "unreachable"; } handle (e) { say "caught: " + e; }
say a;
attempt { local m = {"x": 1}; delete(m, "x"); say m; say m["x"]; } handle (e) { say "caught: " + e; }
local i = 0;
during i < 3 {
    attempt { delete(a, 10); } handle (e) { say "again: " + e; }
    i++;
}
say "end";
