// loop variable no longer leaks: outer x, same name, must be untouched
let x = 9;
for (x in [1, 2, 3]) {
    print x;
}
print x;

// nested for loops with the same variable name at each level -- each
// level's binding is independent
for (n in [1, 2]) {
    for (n in [10, 20]) {
        print n;
    }
    print n;
}
