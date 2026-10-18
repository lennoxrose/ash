local total = 0;
local j = 0;
during j < 10 {
    j += 1;
    given j == 3 {
        next;
    }
    given j == 7 {
        stop;
    }
    total += j;
}
say total;
say j;

local sum = 0;
each v in [1, 2, 3, 4, 5, 6, 7, 8] {
    given v == 2 {
        next;
    }
    given v == 6 {
        stop;
    }
    sum += v;
}
say sum;

// stop only breaks the innermost loop
local results = [];
each a in [1, 2, 3] {
    each b in [10, 20, 30] {
        given b == 20 {
            next;
        }
        given a == 3 {
            stop;
        }
        local results = push(results, a * b);
    }
}
say results;
