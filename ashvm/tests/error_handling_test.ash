fn risky(n) {
    if (n > 3) {
        let undefined_thing = does_not_exist + 1;
    }
    return n;
}

let result = 0;
try {
    let result = risky(5);
    print "should not reach here";
} catch (e) {
    print "caught: " + e;
}
print "after try 1, result still: " + str(result);

let arr = [1, 2, 3];
try {
    print arr[10];
} catch (e) {
    print "caught: " + e;
}

let m = {"a": 1};
try {
    print m["missing_key"];
} catch (e) {
    print "caught: " + e;
}

fn needs_two(a, b) { return a + b; }
try {
    print needs_two(1);
} catch (e) {
    print "caught: " + e;
}

try {
    let content = read_file("/tmp/does_not_exist_xyz_ash.txt");
} catch (e) {
    print "caught: " + e;
}

try {
    try {
        let x = totally_undefined_var;
    } catch (inner) {
        print "inner caught: " + inner;
    }
    print "inner try/catch resolved, continuing outer try";
} catch (outer) {
    print "outer should not see this: " + outer;
}

print "program completed normally";
let final_check = 42;
print final_check;
