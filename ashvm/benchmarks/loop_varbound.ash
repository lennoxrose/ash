local n = 20000000;
local sum = 0;
local i = 0;
during (i < n) {
    local sum = sum + i;
    local i = i + 1;
}
say sum;
