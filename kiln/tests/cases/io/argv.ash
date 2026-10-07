local args = argv();
say len(args);
say args[0];
each (a in args) {
    say a;
}
