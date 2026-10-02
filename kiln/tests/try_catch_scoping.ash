let e = "outer";
try {
    throw "inner";
} catch (e) {
    print e;
}
print e;

try {
    print "no error";
} catch (e) {
    print "should not run";
}
print e;

// nested try/catch, inner catch variable shadows outer's
try {
    try {
        throw "deep";
    } catch (e) {
        print e;
    }
    print e;
} catch (e) {
    print "outer should not run";
}
print e;
