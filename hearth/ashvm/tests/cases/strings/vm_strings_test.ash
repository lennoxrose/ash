local name = "Ash";
say "Hello, " + name + "!";

local a = "abc";
local b = "abc";
say a == b;
say a == "xyz";

forge greet(who) {
    yield "Hi " + who;
}
say greet("world");
