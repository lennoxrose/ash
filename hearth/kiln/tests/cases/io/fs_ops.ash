local d = "/tmp/ash_fs_test_dir";
delete_file(d + "/b.txt"); delete_file(d + "/a.txt"); delete_file(d + "/c.txt");
make_dir(d);
say file_exists(d + "/a.txt");
write_file(d + "/a.txt", "hello");
write_file(d + "/b.txt", "world");
say rename_file(d + "/b.txt", d + "/c.txt");
say rename_file(d + "/nope.txt", d + "/x.txt");
say sort(list_dir(d));
say read_file(d + "/c.txt");
say delete_file(d + "/a.txt");
say delete_file(d + "/a.txt");
say delete_file(d + "/c.txt");
say list_dir(d);
attempt { list_dir(d + "/missing"); } handle (e) { say "no dir"; }
