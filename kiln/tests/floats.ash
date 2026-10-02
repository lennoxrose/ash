print 10 / 4;
print 0.5;
print -3.25;
print 1.5 + 2.25;
print 0.1 + 0.2;

let x = 2.5;
print x < 3;
print x > 3;

fn avg(a, b) {
    return (a + b) / 2;
}
print avg(1, 2);

fn half(n) {
    if (n < 1) {
        return n;
    }
    return half(n / 2) + 0.1;
}
print half(4);
