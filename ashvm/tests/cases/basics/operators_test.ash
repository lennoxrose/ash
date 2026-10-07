local x = 10;
x += 5;
say x;
x -= 3;
say x;
x *= 2;
say x;
x /= 4;
say x;

local s = "hello";
s += " world";
say s;

local i = 0;
i++;
i++;
i--;
say i;

local n = 7;
say n > 5 ? "big" : "small";
say n < 5 ? "big" : n < 10 ? "medium" : "huge";

say 5 & 3;
say 5 | 2;
say 5 ^ 1;
say ~5;
say 1 << 4;
say 256 >> 4;
say (5 & 3) == 1;
