attempt { say "x" + 1; } handle (e) { say "caught plus"; }
attempt { say 1 + "x"; } handle (e) { say "caught plus2"; }
attempt { say str("text"); } handle (e) { say "caught str"; }
say "a" + "b";
say 1 + 2;
say str(5);
