// closure created and returned from inside an given-block, capturing a
// block-scoped variable; call it after the block, once its slot has
// been reused by a later declaration
forge make_from_if() {
    local f = 0;
    given (yes) {
        local x = 42;
        f = forge() { yield x; };
    }
    local after = 999; // reuses x's old slot
    yield f;
}
local cf = make_from_if();
say cf();

// closure capturing a variable that shadows an outer same-named one --
// the closure must see the INNER value; the outer variable must be
// untouched afterward
forge make_shadow() {
    local x = 1;
    local f = 0;
    given (yes) {
        local x = 2;
        f = forge() { yield x; };
    }
    yield [x, f()];
}
local r = make_shadow();
say r[0];
say r[1];

// closures created inside a during body (compiled once, run 3 times),
// each capturing that iteration's own block-scoped value
forge make_list() {
    local fns = [];
    local i = 0;
    during (i < 3) {
        local captured = i;
        push(fns, forge() { yield captured; });
        i = i + 1;
    }
    yield fns;
}
local list = make_list();
local l0 = list[0];
local l1 = list[1];
local l2 = list[2];
say l0();
say l1();
say l2();
