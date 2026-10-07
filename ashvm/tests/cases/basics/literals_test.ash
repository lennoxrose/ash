say yes;
say no;
say none;

say yes == 1;
say no == 0;
say none == none;
say none == 0;
say none == no;

given yes {
    say "yes is truthy";
}
given no {
    say "unreachable";
} otherwise {
    say "no is falsy";
}
local x = none;
given x {
    say "unreachable";
} otherwise {
    say "none is falsy";
}

local arr = [1, none, yes, no];
say arr;
local m = {"a": none, "b": yes};
say m;
