say 10 / 4;
say 0.5;
say -3.25;
say 1.5 + 2.25;
say 0.1 + 0.2;

local x = 2.5;
say x < 3;
say x > 3;

forge avg(a, b) {
    yield (a + b) / 2;
}
say avg(1, 2);

forge half(n) {
    given (n < 1) {
        yield n;
    }
    yield half(n / 2) + 0.1;
}
say half(4);
