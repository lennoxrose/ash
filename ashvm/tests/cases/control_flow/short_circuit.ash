local a = [1, 2];
given 5 < len(a) and a[5] == 1 { say "bad"; } otherwise { say "ok and"; }
given 0 < len(a) or a[5] == 1 { say "ok or"; }
say 1 and 2;
say 0 or 3;
say 0 and 1;
say 1 or 0;
say 1 and 0 or 1;
local n = 0;
during n < 5 and n != 3 { n++; }
say n;
