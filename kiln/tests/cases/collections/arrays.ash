let a = [1, 2, 3];
print a[0];
print a[1];
print a[2];
print len(a);

a[0] = 99;
print a[0];

let b = ["x", "y", "z"];
print b[1];

let c = [1];
push(c, 2);
push(c, 3);
print len(c);
print c[0];
print c[1];
print c[2];

let d = [];
let i = 0;
while (i < 10) {
    push(d, i);
    i = i + 1;
}
print len(d);
print d[9];
