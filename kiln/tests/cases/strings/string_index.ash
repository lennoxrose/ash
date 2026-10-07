local s = "hello";
say s[0];
say s[4];
say s[1] + s[2];

attempt {
    say s[10];
} handle (e) {
    say "caught: " + e;
}

attempt {
    s[0] = "H";
} handle (e) {
    say "caught: " + e;
}
say "done";
