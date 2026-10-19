package t;

import ash.util.ArrayList;
import ash.util.List;

interface Op { int apply(int x); }

class Account {
    private final int[] history;
    private int pos;

    Account(int size) {
        history = new int[size];
        record(size);
    }

    void record(int v) {
        history[pos] = v;
        pos++;
    }

    int ratio(int a, int b) {
        return a / b;
    }
}

public class Stack1 {
    static int run(Op op, int v) {
        return op.apply(v);
    }

    static int viaLambda(Account acc, int d) {
        return run(x -> acc.ratio(x, d), 10);
    }

    static void deep(int n, Account acc) {
        if (n == 0) {
            System.out.println(viaLambda(acc, 0));
            return;
        }
        deep(n - 1, acc);
    }

    public static void main(String[] args) {
        Account acc = new Account(2);
        acc.record(5);
        List<Integer> seen = new ArrayList<>();
        seen.add(deepCount(3));
        System.out.println("before the failure " + seen.size());
        deep(3, acc);
        System.out.println("not reached");
    }

    static int deepCount(int n) {
        return n <= 0 ? 0 : 1 + deepCount(n - 1);
    }
}
