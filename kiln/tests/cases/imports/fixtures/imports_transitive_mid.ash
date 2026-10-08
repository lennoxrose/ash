@import <./imports_basic_lib.ash>;

forge mid_triple(x) { yield imports_basic_lib.add(x, imports_basic_lib.add(x, x)); }
