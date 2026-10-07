print "line one\nline two";
print "tab\there";
print "quote: \"hello\"";
print "backslash: \\";
write_file("/tmp/ash_escape_test.txt", "Hello from Ash!\n");
let content = read_file("/tmp/ash_escape_test.txt");
print content;
append_file("/tmp/ash_escape_test.txt", "Second line\n");
print read_file("/tmp/ash_escape_test.txt");
