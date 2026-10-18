local m = {"a": 1, "b": 2, "c": 3};
local ks = keys(m);
local vs = values(m);
say len(ks);
say len(vs);
say ks[0];
say ks[1];
say ks[2];
say vs[0];
say vs[1];
say vs[2];

local empty_m = {};
say len(keys(empty_m));
say len(values(empty_m));
