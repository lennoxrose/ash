// A fresh `local` declared inside a re-executed block must not leak across
// iterations/branches -- found via a cross-engine audit against kiln.

local i = 0;
during i < 3 {
    local x = i * 10;
    say x;
    i += 1;
}

local arr = [1, 2, 3];
each n in arr {
    local y = n * 10;
    say y;
}

local j = 0;
during j < 6 {
    given j % 2 == 0 {
        local a = j * 100;
        say a;
    } otherwise {
        j += 1;
        next;
    }
    j += 1;
}

each x in arr {
    given x == 2 { next; }
    local z = x * 1000;
    say z;
}

each outer in [1, 2] {
    local tmp = outer;
    each inner in [10, 20] {
        local tmp2 = outer + inner;
        say tmp2;
    }
    say tmp;
}
