// closure created and returned from inside an if-block, capturing a
// block-scoped variable; call it after the block, once its slot has
// been reused by a later declaration
fn make_from_if() {
    let f = 0;
    if (true) {
        let x = 42;
        f = fn() { return x; };
    }
    let after = 999; // reuses x's old slot
    return f;
}
let cf = make_from_if();
print cf();

// closure capturing a variable that shadows an outer same-named one --
// the closure must see the INNER value; the outer variable must be
// untouched afterward
fn make_shadow() {
    let x = 1;
    let f = 0;
    if (true) {
        let x = 2;
        f = fn() { return x; };
    }
    return [x, f()];
}
let r = make_shadow();
print r[0];
print r[1];

// closures created inside a while body (compiled once, run 3 times),
// each capturing that iteration's own block-scoped value
fn make_list() {
    let fns = [];
    let i = 0;
    while (i < 3) {
        let captured = i;
        push(fns, fn() { return captured; });
        i = i + 1;
    }
    return fns;
}
let list = make_list();
let l0 = list[0];
let l1 = list[1];
let l2 = list[2];
print l0();
print l1();
print l2();
