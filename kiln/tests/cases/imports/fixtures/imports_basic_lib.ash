forge helper(x) {
    yield x * 2;
}

forge add(a, b) {
    yield a + b + helper(1);
}
