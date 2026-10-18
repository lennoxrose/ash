local x = 1;
given (yes) {
    local x = 2;
    say x;
}
say x;

local y = 10;
during (y < 13) {
    local z = y * 2;
    say z;
    y = y + 1;
}

// 20 given-blocks, each declaring 4 UNIQUE-named variables (80 total) --
// leaking would exceed MAX_KILN_VARS (64) and fail to compile; scoped
// slot reuse keeps this well under the cap. Verified during planning:
// this exact file (with the shadowing local above already showing the
// bug) fails pre-fix with "error: too many variables" at block index 15.
local count = 0;
given (yes) { local a0=1; local b0=2; local c0=3; local d0=4; count = count + 1; }
given (yes) { local a1=1; local b1=2; local c1=3; local d1=4; count = count + 1; }
given (yes) { local a2=1; local b2=2; local c2=3; local d2=4; count = count + 1; }
given (yes) { local a3=1; local b3=2; local c3=3; local d3=4; count = count + 1; }
given (yes) { local a4=1; local b4=2; local c4=3; local d4=4; count = count + 1; }
given (yes) { local a5=1; local b5=2; local c5=3; local d5=4; count = count + 1; }
given (yes) { local a6=1; local b6=2; local c6=3; local d6=4; count = count + 1; }
given (yes) { local a7=1; local b7=2; local c7=3; local d7=4; count = count + 1; }
given (yes) { local a8=1; local b8=2; local c8=3; local d8=4; count = count + 1; }
given (yes) { local a9=1; local b9=2; local c9=3; local d9=4; count = count + 1; }
given (yes) { local a10=1; local b10=2; local c10=3; local d10=4; count = count + 1; }
given (yes) { local a11=1; local b11=2; local c11=3; local d11=4; count = count + 1; }
given (yes) { local a12=1; local b12=2; local c12=3; local d12=4; count = count + 1; }
given (yes) { local a13=1; local b13=2; local c13=3; local d13=4; count = count + 1; }
given (yes) { local a14=1; local b14=2; local c14=3; local d14=4; count = count + 1; }
given (yes) { local a15=1; local b15=2; local c15=3; local d15=4; count = count + 1; }
given (yes) { local a16=1; local b16=2; local c16=3; local d16=4; count = count + 1; }
given (yes) { local a17=1; local b17=2; local c17=3; local d17=4; count = count + 1; }
given (yes) { local a18=1; local b18=2; local c18=3; local d18=4; count = count + 1; }
given (yes) { local a19=1; local b19=2; local c19=3; local d19=4; count = count + 1; }
say count;
