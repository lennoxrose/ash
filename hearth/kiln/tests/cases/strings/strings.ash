say "hello";
say "hello, " + "ash!";

local s = "x";
say s == "x";
say s == "y";
say s != "x";

local a = "foo";
local b = "bar";
say a + b;

say "line1\nline2";
say "quote: \"hi\"";

forge greet(name) {
    yield "hello, " + name;
}
say greet("world");

local x = "a";
x = x + "b";
x = x + "c";
say x;

forge repeat(s, n) {
    given (n <= 0) {
        yield "";
    }
    yield s + repeat(s, n - 1);
}
say repeat("ab", 5);

local n = 5;
say n;
say s + "test";
say n + 1;
