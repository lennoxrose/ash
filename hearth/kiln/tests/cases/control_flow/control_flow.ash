local x = 5;
x = x + 1;
say x;

given (x > 5) {
    say 1;
} otherwise {
    say 0;
}

local i = 0;
during (i < 3) {
    say i;
    i = i + 1;
}

say 1 == 1;
say 1 != 1;
say (1 < 2) and (3 > 2);
say (1 > 2) or (3 < 2);
