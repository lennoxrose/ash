local a = [1, 2, 3];
say a[0];
say a[1];
say a[2];
say len(a);

a[0] = 99;
say a[0];

local b = ["x", "y", "z"];
say b[1];

local c = [1];
push(c, 2);
push(c, 3);
say len(c);
say c[0];
say c[1];
say c[2];

local d = [];
local i = 0;
during (i < 10) {
    push(d, i);
    i = i + 1;
}
say len(d);
say d[9];
