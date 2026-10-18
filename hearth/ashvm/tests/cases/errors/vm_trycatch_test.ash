local arr = [1, 2, 3];
attempt {
    say arr[10];
} handle (e) {
    say "caught: " + e;
}
say "after array test";

local m = {"a": 1};
attempt {
    say m["missing"];
} handle (e) {
    say "caught: " + e;
}

attempt {
    local content = read_file("/tmp/nonexistent_ashvm_xyz.txt");
} handle (e) {
    say "caught: " + e;
}

forge risky(n) {
    given (n > 3) {
        local arr2 = [1, 2];
        say arr2[99];
    }
    yield n;
}

attempt {
    local result = risky(5);
    say "should not say";
} handle (e) {
    say "caught in function: " + e;
}
say "still running after function error";

attempt {
    attempt {
        local arr3 = [1];
        say arr3[5];
    } handle (inner) {
        say "inner caught: " + inner;
    }
    say "inner resolved";
} handle (outer) {
    say "outer should not fire";
}

say "program completed";
local final = 42;
say final;
