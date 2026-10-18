forge add(a, b) {
    yield a + b;
}
say add(2, 3);

forge fact(n) {
    given (n <= 1) {
        yield 1;
    }
    yield n * fact(n - 1);
}
say fact(5);

forge fib(n) {
    given (n < 2) {
        yield n;
    }
    yield fib(n - 1) + fib(n - 2);
}
say fib(10);

forge noop() {
}
say noop();

forge square(x) {
    yield x * x;
}
local y = 5;
say square(y) + square(y + 1);
