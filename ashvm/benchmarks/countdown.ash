local i = 20000000;
local count = 0;
during (i > 0) {
    local count = count + 1;
    local i = i - 1;
}
say count;
