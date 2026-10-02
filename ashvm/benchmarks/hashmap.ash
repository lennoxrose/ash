let m = {};
let i = 0;
while (i < 50000) {
    m[str(i)] = i * 2;
    let i = i + 1;
}
let j = 0;
let total = 0;
while (j < 50000) {
    let total = total + m[str(j)];
    let j = j + 1;
}
print total;
