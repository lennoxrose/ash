local a = [1, 2, 3, 4];
delete(a, 1);
say a;
say pop(a);
say a;
insert(a, 0, 9);
insert(a, len(a), 7);
insert(a, 1, 8);
say a;
say slice(a, 1, 3);
say slice(a, 0, 0);
attempt { pop([]); } handle (e) { say "empty"; }
attempt { delete(a, 10); } handle (e) { say "oob"; }
local m = {"k": 1};
delete(m, "k");
say len(m);
