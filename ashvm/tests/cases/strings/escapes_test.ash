say "line one\nline two";
say "tab\there";
say "quote: \"hello\"";
say "backslash: \\";
write_file("/tmp/ash_escape_test.txt", "Hello from Ash!\n");
local content = read_file("/tmp/ash_escape_test.txt");
say content;
append_file("/tmp/ash_escape_test.txt", "Second line\n");
say read_file("/tmp/ash_escape_test.txt");
