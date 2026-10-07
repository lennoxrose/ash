local x = 5;
x += 3;
say x;
x -= 2;
say x;
x *= 4;
say x;
x /= 3;
say x;

local s = "hello";
s += " world";
say s;

local i = 0;
i++;
i++;
say i;
i--;
say i;

local total = 0;
each (n in [1,2,3,4,5]) {
    total += n;
}
say total;

local j = 0;
during (j < 5) {
    j++;
}
say j;
