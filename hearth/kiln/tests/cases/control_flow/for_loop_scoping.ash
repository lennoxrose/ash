// loop variable no longer leaks: outer x, same name, must be untouched
local x = 9;
each (x in [1, 2, 3]) {
    say x;
}
say x;

// nested each loops with the same variable name at each level -- each
// level's binding is independent
each (n in [1, 2]) {
    each (n in [10, 20]) {
        say n;
    }
    say n;
}
