#define N 2000

double** create_matrix(int size, double value)
{
    double** mat = new double*[size];

    for (int i = 0; i < size; i++) {
        mat[i] = new double[size];

        for (int j = 0; j < size; j++) {
            mat[i][j] = value;
        }
    }

    return mat;
}

void free_matrix(double** mat, int size)
{
    for (int i = 0; i < size; i++) {
        delete[] mat[i];
    }

    delete[] mat;
}

int main()
{
    double** A = create_matrix(N, 1.5);
    double** B = create_matrix(N, 2.0);
    double** C = create_matrix(N, 0.0);

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double sum = 0.0;

            for (int k = 0; k < N; k++) {
                sum += A[i][k] * B[k][j];
            }

            C[i][j] = sum;
        }
    }

    // Prevent the compiler from treating the calculation as unused.
    volatile double result = C[0][0];

    free_matrix(A, N);
    free_matrix(B, N);
    free_matrix(C, N);

    return result == 1500.0 ? 0 : 1;
}