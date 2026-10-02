#include <cstdio>
int main() {
    long long sum = 0;
    long long N = 20000000;
    for (long long i = 0; i < N; i++) {
        sum += i;
    }
    printf("%lld\n", sum);
    return 0;
}
