package examples;

import ash.lang.function.BiFunction;
import ash.lang.function.Function;
import ash.lang.function.Predicate;
import ash.lang.function.Supplier;
import ash.util.ArrayList;
import ash.util.List;

public class Lambdas {
    static Function<Integer, Integer> adder(int n) {
        return x -> x + n;
    }

    static <T> List<T> keep(List<T> xs, Predicate<T> test) {
        List<T> out = new ArrayList<>();
        for (T x : xs) {
            if (test.test(x)) {
                out.add(x);
            }
        }
        return out;
    }

    static int twice(int x) { return x * 2; }

    public static void main(String[] args) {
        Function<Integer, Integer> add5 = adder(5);
        BiFunction<Integer, Integer, Integer> mul = (a, b) -> a * b;
        Supplier<String> greeting = () -> "hi";
        Function<Integer, Integer> viaRef = Lambdas::twice;
        System.out.println(add5.apply(3) + " " + mul.apply(6, 7) + " " + greeting.get() + " " + viaRef.apply(8));
        List<String> names = new ArrayList<>();
        names.add("ash");
        names.add("kiln");
        names.add("pyre");
        List<String> four = keep(names, s -> s.length() == 4);
        System.out.println(four.size() + " " + four.get(0));
        int base = 10;
        Function<Integer, Function<Integer, Integer>> curried = a -> b -> a + b + base;
        System.out.println(curried.apply(1).apply(2));
    }
}
