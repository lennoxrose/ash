let parts = split("a,b,c", ",");
print parts;
print join(parts, "-");
print substring("hello world", 0, 5);
print indexOf("hello world", "world");
print indexOf("hello world", "xyz");
print replace("foo bar foo", "foo", "baz");
print upper("hello");
print lower("WORLD");
print trim("   spaced   ");

write_file("/tmp/ash_test.txt", "Hello from Ash!\n");
print file_exists("/tmp/ash_test.txt");
let content = read_file("/tmp/ash_test.txt");
print content;
append_file("/tmp/ash_test.txt", "Second line\n");
let content2 = read_file("/tmp/ash_test.txt");
print content2;
print file_exists("/tmp/nonexistent_file_xyz.txt");
