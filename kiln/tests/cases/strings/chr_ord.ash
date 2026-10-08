say ord("A");
say chr(97) + chr(98);
say ord(chr(200));
say chr(ord("z") - 1);
local s = "hello";
local i = 0;
during i < len(s) { say ord(substring(s, i, i + 1)); i++; }
