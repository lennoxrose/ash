local path = "/tmp/kiln_file_test.txt";
say file_exists(path);
say write_file(path, "hello, file!\nsecond line");
say file_exists(path);
say read_file(path);
say append_file(path, "\nappended");
say read_file(path);
