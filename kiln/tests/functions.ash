fn add(a, b) {
    return a + b;
}
print add(2, 3);

fn fact(n) {
    if (n <= 1) {
        return 1;
    }
    return n * fact(n - 1);
}
print fact(5);

fn fib(n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
print fib(10);

fn noop() {
}
print noop();

fn square(x) {
    return x * x;
}
let y = 5;
print square(y) + square(y + 1);
