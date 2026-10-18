@import <./fixtures/imports_state_lib.ash>;

say imports_state_lib.bump();
say imports_state_lib.bump();
say imports_state_lib.hexdigit(11);
say imports_state_lib.table_at(1);
say imports_state_lib.add_item(40);
say imports_state_lib.add_item(50);
say imports_state_lib.TABLE;
imports_state_lib.set_level(9);
say imports_state_lib.level();
say imports_state_lib.CONFIG["name"];
say imports_state_lib.DOUBLE;
say imports_state_lib.ENABLED;
imports_state_lib.reset();
say imports_state_lib.bump2();
say imports_state_lib.COUNT;
