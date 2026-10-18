local parts = split("a,b,c", ",");
say len(parts);
say parts[0];
say parts[1];
say parts[2];

local chars = split("abc", "");
say len(chars);
say chars[0];
say chars[2];

local empty1 = split("", ",");
say len(empty1);
say empty1[0];

local empty2 = split("", "");
say len(empty2);

say join(["a", "b", "c"], "-");
say join(["x"], ",");
say join([], ",");

say replace("hello world", "world", "there");
say replace("aaa", "a", "bb");
say replace("hello", "xyz", "abc");
say replace("hello", "", "X");
say replace("banana", "a", "o");
