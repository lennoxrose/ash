forge f() {
    local i = 0;
    during i < 3 {
        attempt { local x = 1; } handle (e) { say "inner"; }
        i++;
    }
    raise "boom";
}
attempt { f(); } handle (e) { say "outer: " + e; }
forge g() {
    attempt { yield 5; } handle (e) { say "no"; }
}
forge h() { local v = g(); raise "boom2"; }
attempt { h(); } handle (e) { say "outer: " + e; }
local i = 0;
during i < 3 {
    i++;
    attempt { next; } handle (e) { say "no"; }
}
attempt { raise "second"; } handle (e) { say "caught " + e; }
say "done";
