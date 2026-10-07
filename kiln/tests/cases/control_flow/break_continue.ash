let i = 0;
while (i < 10) {
    i = i + 1;
    if (i == 5) { break; }
    print i;
}
print "---";

let j = 0;
while (j < 5) {
    j = j + 1;
    if (j == 3) { continue; }
    print j;
}
print "---";

for (x in [1,2,3,4,5]) {
    if (x == 3) { break; }
    print x;
}
print "---";

for (x in [1,2,3,4,5]) {
    if (x == 3) { continue; }
    print x;
}
print "---";

// nested loops: break only exits innermost
for (i in [1,2]) {
    for (j in [1,2,3]) {
        if (j == 2) { break; }
        print i * 10 + j;
    }
}
print "---";

// break/continue inside function's own loop, not affecting caller
fn find_first_even(arr) {
    for (x in arr) {
        if (x - floor(x/2)*2 == 0) {
            return x;
        }
    }
    return -1;
}
print find_first_even([1, 3, 5, 4, 7]);
