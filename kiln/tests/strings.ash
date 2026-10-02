print "hello";
print "hello, " + "ash!";

let s = "x";
print s == "x";
print s == "y";
print s != "x";

let a = "foo";
let b = "bar";
print a + b;

print "line1\nline2";
print "quote: \"hi\"";

fn greet(name) {
    return "hello, " + name;
}
print greet("world");

let x = "a";
x = x + "b";
x = x + "c";
print x;

fn repeat(s, n) {
    if (n <= 0) {
        return "";
    }
    return s + repeat(s, n - 1);
}
print repeat("ab", 5);

let n = 5;
print n;
print s + "test";
print n + 1;
