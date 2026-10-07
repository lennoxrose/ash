attempt {
    raise "custom error!";
} handle (e) {
    say "caught: " + e;
}
say "after";

forge validate(x) {
    given (x < 0) {
        raise "negative value: " + str(x);
    }
    yield x;
}

attempt {
    say validate(5);
    say validate(-3);
    say "unreachable";
} handle (e) {
    say "caught: " + e;
}
say "done";
