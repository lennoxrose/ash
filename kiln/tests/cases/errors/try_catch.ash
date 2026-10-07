// basic catch of an out-of-bounds array error, execution continues after
let a = [1, 2, 3];
try {
    print a[10];
} catch (e) {
    print "caught 1";
}
print "after 1";

// catch of a missing-map-key error
let m = {"x": 1};
try {
    print m["missing"];
} catch (e) {
    print "caught 2";
}
print "after 2";

// no error raised -- the try body's own value still prints, catch is skipped
try {
    print 42;
} catch (e) {
    print "should not print";
}

// nested try/catch: inner catches its own error, outer never sees it
try {
    try {
        print a[99];
    } catch (e) {
        print "inner caught";
    }
    print "after inner";
} catch (e) {
    print "outer should not run";
}

// error raised inside a function called from within a try -- unwinds
// through the call
fn boom() {
    let z = [1];
    return z[5];
}
try {
    boom();
} catch (e) {
    print "caught from function";
}
print "done";
