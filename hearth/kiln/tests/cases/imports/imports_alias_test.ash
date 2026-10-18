@import <./fixtures/imports_basic_lib.ash> as lib;
@import <./fixtures/imports_const_lib.ash> as k;

say lib.add(3, 4);
say lib.helper(10);
say k.GREETING;
