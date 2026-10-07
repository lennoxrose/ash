say yes;
say no;
say none;
say yes == 1;
say no == 0;
say none == none;
say none == 0;
say none == no;
given (yes) { say "yes"; }
given (no) { say "no"; } otherwise { say "else"; }
local x = none;
given (x) { say "truthy"; } otherwise { say "falsy nil"; }
local y = yes;
say y;
