local parts = split("a,b,c", ",");
say parts;
say join(parts, "-");
say substring("hello world", 0, 5);
say indexOf("hello world", "world");
say indexOf("hello world", "xyz");
say replace("foo bar foo", "foo", "baz");
say upper("hello");
say lower("WORLD");
say trim("   spaced   ");

write_file("/tmp/ash_test.txt", "Hello from Ash!\n");
say file_exists("/tmp/ash_test.txt");
local content = read_file("/tmp/ash_test.txt");
say content;
append_file("/tmp/ash_test.txt", "Second line\n");
local content2 = read_file("/tmp/ash_test.txt");
say content2;
say file_exists("/tmp/nonexistent_file_xyz.txt");
