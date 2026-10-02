let s = "hello";
print s[0];
print s[4];
print s[1] + s[2];

try {
    print s[10];
} catch (e) {
    print "caught: " + e;
}

try {
    s[0] = "H";
} catch (e) {
    print "caught: " + e;
}
print "done";
