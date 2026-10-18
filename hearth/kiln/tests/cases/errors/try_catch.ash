// basic handle of an out-of-bounds array error, execution continues after
local a = [1, 2, 3];
attempt {
    say a[10];
} handle (e) {
    say "caught 1";
}
say "after 1";

// handle of a missing-map-key error
local m = {"x": 1};
attempt {
    say m["missing"];
} handle (e) {
    say "caught 2";
}
say "after 2";

// no error raised -- the attempt body's own value still prints, handle is skipped
attempt {
    say 42;
} handle (e) {
    say "should not print";
}

// nested attempt/handle: inner catches its own error, outer never sees it
attempt {
    attempt {
        say a[99];
    } handle (e) {
        say "inner caught";
    }
    say "after inner";
} handle (e) {
    say "outer should not run";
}

// error raised inside a function called from within a attempt -- unwinds
// through the call
forge boom() {
    local z = [1];
    yield z[5];
}
attempt {
    boom();
} handle (e) {
    say "caught from function";
}
say "done";
