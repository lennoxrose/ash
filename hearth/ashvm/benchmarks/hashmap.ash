local m = {};
local i = 0;
during (i < 50000) {
    m[str(i)] = i * 2;
    local i = i + 1;
}
local j = 0;
local total = 0;
during (j < 50000) {
    local total = total + m[str(j)];
    local j = j + 1;
}
say total;
