let path = "/tmp/kiln_file_test.txt";
print file_exists(path);
print write_file(path, "hello, file!\nsecond line");
print file_exists(path);
print read_file(path);
print append_file(path, "\nappended");
print read_file(path);
