#include <cstdio>
int main() {
    long long i = 20000000;
    long long count = 0;
    while (i > 0) {
        count += 1;
        i -= 1;
    }
    printf("%lld\n", count);
    return 0;
}
