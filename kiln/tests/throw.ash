try {
    throw "custom error!";
} catch (e) {
    print "caught: " + e;
}
print "after";

fn validate(x) {
    if (x < 0) {
        throw "negative value: " + str(x);
    }
    return x;
}

try {
    print validate(5);
    print validate(-3);
    print "unreachable";
} catch (e) {
    print "caught: " + e;
}
print "done";
