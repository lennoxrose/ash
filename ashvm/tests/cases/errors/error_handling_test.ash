forge risky(n) {
    given (n > 3) {
        local undefined_thing = does_not_exist + 1;
    }
    yield n;
}

local result = 0;
attempt {
    local result = risky(5);
    say "should not reach here";
} handle (e) {
    say "caught: " + e;
}
say "after attempt 1, result still: " + str(result);

local arr = [1, 2, 3];
attempt {
    say arr[10];
} handle (e) {
    say "caught: " + e;
}

local m = {"a": 1};
attempt {
    say m["missing_key"];
} handle (e) {
    say "caught: " + e;
}

forge needs_two(a, b) { yield a + b; }
attempt {
    say needs_two(1);
} handle (e) {
    say "caught: " + e;
}

attempt {
    local content = read_file("/tmp/does_not_exist_xyz_ash.txt");
} handle (e) {
    say "caught: " + e;
}

attempt {
    attempt {
        local x = totally_undefined_var;
    } handle (inner) {
        say "inner caught: " + inner;
    }
    say "inner attempt/handle resolved, continuing outer attempt";
} handle (outer) {
    say "outer should not see this: " + outer;
}

say "program completed normally";
local final_check = 42;
say final_check;
