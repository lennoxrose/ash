package memory;

/// Allocates well past the old fixed 16MB bump-heap cap (each iteration leaks a fresh 4-element
/// array, nothing is ever freed) to exercise the allocator's dynamic growth instead of segfaulting.
class HeapGrowth {
    public static void main(String[] args) {
        int total = 0;
        int i = 0;
        while (i < 2000000) {
            int[] arr = { i, i + 1, i + 2, i + 3 };
            total = total + arr[0];
            i = i + 1;
        }
        System.out.println(total);
    }
}
