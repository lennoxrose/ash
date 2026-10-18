local nums = [10, 20, 30, 40, 50];

each x in nums {
    say x;
}

each (y in nums) {
    say y * 2;
}

local m = {"a": 1, "b": 2, "c": 3};
each k in keys(m) {
    say k + " = " + str(m[k]);
}

local total = 0;
each n in nums {
    local total = total + n;
}
say total;

local empty = [];
local empty_count = 0;
each e in empty {
    local empty_count = empty_count + 1;
}
say empty_count;

local rows = [[1, 2], [3, 4], [5, 6]];
local line = "";
each row in rows {
    local line = "";
    each cell in row {
        local line = line + str(cell) + " ";
    }
    say line;
}

local outer_total = 0;
each a in [1, 2, 3] {
    each b in [10, 20] {
        local outer_total = outer_total + a * b;
    }
}
say outer_total;
