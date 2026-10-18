say contains("hello world", "o w");
say contains("hello", "xyz");
say contains("hello", "");
say starts_with("hello", "he");
say starts_with("hello", "hello!");
say starts_with("hello", "");
say starts_with("hello", "el");
say ends_with("hello", "llo");
say ends_with("hello", "hell");
say ends_with("hello", "");
say ends_with("lo", "hello");
say repeat("ab", 3);
say repeat("x", 0);
say repeat("", 5) + "|";
say len(repeat("abc", 1000));
attempt { repeat("a", -1); } handle (e) { say "neg"; }
