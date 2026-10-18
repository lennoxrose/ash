local DIGITS = "0123456789abcdef";
local TABLE = [10, 20, 30];
local COUNT = 0;
local CONFIG = {"name": "stateful", "level": 2};
local DOUBLE = 21 * 2;
local ENABLED = yes;

forge bump() { COUNT += 1; yield COUNT; }
forge hexdigit(n) { yield substring(DIGITS, n, n + 1); }
forge table_at(i) { yield TABLE[i]; }
forge add_item(v) { push(TABLE, v); yield len(TABLE); }
forge set_level(l) { CONFIG["level"] = l; }
forge level() { yield CONFIG["level"]; }
forge reset() { COUNT = 0; }
forge bump2() { COUNT++; yield COUNT; }
