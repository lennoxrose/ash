local e = "outer";
attempt {
    raise "inner";
} handle (e) {
    say e;
}
say e;

attempt {
    say "no error";
} handle (e) {
    say "should not run";
}
say e;

// nested attempt/handle, inner handle variable shadows outer's
attempt {
    attempt {
        raise "deep";
    } handle (e) {
        say e;
    }
    say e;
} handle (e) {
    say "outer should not run";
}
say e;
