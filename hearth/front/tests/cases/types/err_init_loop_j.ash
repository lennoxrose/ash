package t;

// expect: variable 'c' is used before it is assigned: the first pass through the loop
// expect: variable 'd' is used before it is assigned: the first pass through the loop
public class ErrInitLoop {
    static int carried(int n) {
        int c;
        int sum = 0;
        for (int i = 0; i < n; i++) {
            sum += c;
            c = i;
        }
        return sum;
    }

    static int inCondition() {
        int d;
        int k = 0;
        while (d < 3) {
            d = k;
            k++;
        }
        return k;
    }
}
