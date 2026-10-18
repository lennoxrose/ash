// Goal to beat
// 
// use the "time" command to measure the execution time of the program
//
// matrix_multiply.cpp time (compiled with g++ at -O2) output:
//real    0m5.761s
//user    0m5.721s
//sys     0m0.037s
//
// CPU: AMD Ryzen 7 7800X 8-Core Processor
// GPU: AMD Radeon RX 9070 XT 16GB
// 
// if you cant beat it by atleast 50% then you are not allowed to submit your solution

// Matrix multiplication: C = A x B
// Dimensions: N x N
package benchmarks;

class MatrixMultiply {
    static double[][] createMatrix(int size, double value) {
        double[][] mat = new double[size][];
        int i = 0;
        while (i < size) {
            double[] row = new double[size];
            int j = 0;
            while (j < size) {
                row[j] = value;
                j += 1;
            }
            mat[i] = row;
            i += 1;
        }
        return mat;
    }

    public static void main(String[] args) {
        int n = 2000;

        // 1. Initialize two N x N matrices
        double[][] a = createMatrix(n, 1.5);
        double[][] b = createMatrix(n, 2.0);

        // 2. Initialize the result matrix C with zeros
        double[][] c = createMatrix(n, 0.0);

        System.out.println("Starting matrix multiplication (" + n + "x" + n + ")...");

        // 3. Compute C = A x B
        // Total operations: 2 * N^3 = 250,000,000 floating point operations
        int i = 0;
        while (i < n) {
            int j = 0;
            while (j < n) {
                double sum = 0.0;
                int k = 0;
                while (k < n) {
                    sum += a[i][k] * b[k][j];
                    k += 1;
                }
                c[i][j] = sum;
                j += 1;
            }
            i += 1;
        }

        System.out.println("Done!");
        System.out.println("Sample element C[0][0]: " + c[0][0]); // Should be 1500.0
    }
}
