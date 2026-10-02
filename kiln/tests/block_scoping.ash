let x = 1;
if (true) {
    let x = 2;
    print x;
}
print x;

let y = 10;
while (y < 13) {
    let z = y * 2;
    print z;
    y = y + 1;
}

// 20 if-blocks, each declaring 4 UNIQUE-named variables (80 total) --
// leaking would exceed MAX_KILN_VARS (64) and fail to compile; scoped
// slot reuse keeps this well under the cap. Verified during planning:
// this exact file (with the shadowing let above already showing the
// bug) fails pre-fix with "error: too many variables" at block index 15.
let count = 0;
if (true) { let a0=1; let b0=2; let c0=3; let d0=4; count = count + 1; }
if (true) { let a1=1; let b1=2; let c1=3; let d1=4; count = count + 1; }
if (true) { let a2=1; let b2=2; let c2=3; let d2=4; count = count + 1; }
if (true) { let a3=1; let b3=2; let c3=3; let d3=4; count = count + 1; }
if (true) { let a4=1; let b4=2; let c4=3; let d4=4; count = count + 1; }
if (true) { let a5=1; let b5=2; let c5=3; let d5=4; count = count + 1; }
if (true) { let a6=1; let b6=2; let c6=3; let d6=4; count = count + 1; }
if (true) { let a7=1; let b7=2; let c7=3; let d7=4; count = count + 1; }
if (true) { let a8=1; let b8=2; let c8=3; let d8=4; count = count + 1; }
if (true) { let a9=1; let b9=2; let c9=3; let d9=4; count = count + 1; }
if (true) { let a10=1; let b10=2; let c10=3; let d10=4; count = count + 1; }
if (true) { let a11=1; let b11=2; let c11=3; let d11=4; count = count + 1; }
if (true) { let a12=1; let b12=2; let c12=3; let d12=4; count = count + 1; }
if (true) { let a13=1; let b13=2; let c13=3; let d13=4; count = count + 1; }
if (true) { let a14=1; let b14=2; let c14=3; let d14=4; count = count + 1; }
if (true) { let a15=1; let b15=2; let c15=3; let d15=4; count = count + 1; }
if (true) { let a16=1; let b16=2; let c16=3; let d16=4; count = count + 1; }
if (true) { let a17=1; let b17=2; let c17=3; let d17=4; count = count + 1; }
if (true) { let a18=1; let b18=2; let c18=3; let d18=4; count = count + 1; }
if (true) { let a19=1; let b19=2; let c19=3; let d19=4; count = count + 1; }
print count;
