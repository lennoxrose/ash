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
local N = 2000;

forge create_matrix(size, value) {
    local mat = [];
    local i = 0;
    during i < size {
        local row = [];
        local j = 0;
        during j < size {
            push(row, value);
            j += 1;
        }
        push(mat, row);
        i += 1;
    }
    yield mat;
}

// 1. Initialize two N x N matrices
local A = create_matrix(N, 1.5);
local B = create_matrix(N, 2.0);

// 2. Initialize the result matrix C with zeros
local C = create_matrix(N, 0.0);

say "Starting matrix multiplication (" + str(N) + "x" + str(N) + ")...";

// 3. Compute C = A x B
// Total operations: 2 * N^3 = 250,000,000 floating point operations
local i = 0;
during i < N {
    local j = 0;
    during j < N {
        local sum = 0.0;
        local k = 0;
        during k < N {
            sum += A[i][k] * B[k][j];
            k += 1;
        }
        C[i][j] = sum;
        j += 1;
    }
    i += 1;
}

say "Done!";
say "Sample element C[0][0]: " + str(C[0][0]); // Should be 1500