// A lambda with eight nested loops inside nine nested loops: the loops of the lambda are numbered above those around it
// (loop_scope_reset), so the nesting limit of one function body (16) applies to each function separately.
public class run_lambda_deep_loops_j {
    public static void main(String[] args) {
        int[] count = new int[1];
        for (int i0 = 0; i0 < 1; i0++) {
            for (int i1 = 0; i1 < 1; i1++) {
                for (int i2 = 0; i2 < 1; i2++) {
                    for (int i3 = 0; i3 < 1; i3++) {
                        for (int i4 = 0; i4 < 1; i4++) {
                            for (int i5 = 0; i5 < 1; i5++) {
                                for (int i6 = 0; i6 < 1; i6++) {
                                    for (int i7 = 0; i7 < 1; i7++) {
                                        for (int i8 = 0; i8 < 1; i8++) {
                                            Runnable r = () -> {
                for (int j0 = 0; j0 < 1; j0++) {
                    for (int j1 = 0; j1 < 1; j1++) {
                        for (int j2 = 0; j2 < 1; j2++) {
                            for (int j3 = 0; j3 < 1; j3++) {
                                for (int j4 = 0; j4 < 1; j4++) {
                                    for (int j5 = 0; j5 < 1; j5++) {
                                        for (int j6 = 0; j6 < 1; j6++) {
                                            for (int j7 = 0; j7 < 1; j7++) {
                                                count[0] += 1;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            };
            r.run();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        System.out.println(count[0]);
    }
}
