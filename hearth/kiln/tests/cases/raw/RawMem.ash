package raw;

/// raw_alloc/raw_poke/raw_peek/raw_syscall: a byte buffer, its widths and byte order, and a real write(2).
class RawMem {
    @Lower("raw_alloc($1)") private static native int alloc(int n);
    @Lower("raw_free($1)") private static native void free(int p);
    @Lower("raw_peek($1, $2)") private static native int peek(int p, int width);
    @Lower("raw_poke($1, $2, $3)") private static native void poke(int p, int width, int v);
    @Lower("raw_fill($1, $2, $3)") private static native void fill(int p, int v, int count);
    @Lower("raw_copy($1, $2, $3)") private static native void copy(int dst, int src, int n);
    @Lower("raw_syscall($1, $2, $3, $4, 0, 0, 0)") private static native int sys3(int n, int a, int b, int c);

    public static void main(String[] args) {
        int p = alloc(16);
        poke(p, 4, 0x11223344);
        System.out.println(peek(p, 1) == 0x44);
        System.out.println(peek(p, 2) == 0x3344);
        System.out.println(peek(p, 4) == 0x11223344);
        poke(p + 8, 1, 72);
        poke(p + 9, 1, 105);
        poke(p + 10, 1, 10);
        System.out.println(sys3(1, 1, p + 8, 3));       // write(1, "Hi\n", 3) -> 3
        System.out.println(sys3(1, 99, p + 8, 3) < 0);  // write to a bad fd -> -EBADF
        System.out.println(peek(p + 12, 4));            // raw_alloc zeroes the block
        poke(p, 8, 0x01020304);                        // 8 bytes: the upper four stay zero
        System.out.println(peek(p, 8) == 0x01020304);
        System.out.println(peek(p + 4, 4));
        poke(p, 2, 0xAABBCC);                          // only the low 2 bytes are stored
        System.out.println(peek(p, 4) == 0x0102BBCC);
        fill(p, 0x11223344, 3);                        // three dwords
        System.out.println(peek(p, 4) == 0x11223344 && peek(p + 8, 4) == 0x11223344 && peek(p + 12, 4) == 0);
        copy(p + 12, p, 4);                            // disjoint
        System.out.println(peek(p + 12, 4) == 0x11223344);
        poke(p, 1, 1);
        poke(p + 1, 1, 2);
        poke(p + 2, 1, 3);
        poke(p + 3, 1, 4);
        copy(p + 1, p, 3);                             // overlapping, destination above: copied backward
        System.out.println(peek(p, 1) * 1000 + peek(p + 1, 1) * 100 + peek(p + 2, 1) * 10 + peek(p + 3, 1));
        copy(p, p + 1, 3);                             // overlapping, destination below
        System.out.println(peek(p, 1) * 1000 + peek(p + 1, 1) * 100 + peek(p + 2, 1) * 10 + peek(p + 3, 1));
        fill(p, 7, 0);                                 // a count of 0 stores nothing
        System.out.println(peek(p, 4) == 7);
        free(p);
    }
}
