let arr = [1, 2, 3];
try {
    print arr[10];
} catch (e) {
    print "caught: " + e;
}
print "after array test";

let m = {"a": 1};
try {
    print m["missing"];
} catch (e) {
    print "caught: " + e;
}

try {
    let content = read_file("/tmp/nonexistent_ashvm_xyz.txt");
} catch (e) {
    print "caught: " + e;
}

fn risky(n) {
    if (n > 3) {
        let arr2 = [1, 2];
        print arr2[99];
    }
    return n;
}

try {
    let result = risky(5);
    print "should not print";
} catch (e) {
    print "caught in function: " + e;
}
print "still running after function error";

try {
    try {
        let arr3 = [1];
        print arr3[5];
    } catch (inner) {
        print "inner caught: " + inner;
    }
    print "inner resolved";
} catch (outer) {
    print "outer should not fire";
}

print "program completed";
let final = 42;
print final;
