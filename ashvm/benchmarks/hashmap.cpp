#include <cstdio>
#include <unordered_map>
#include <string>
int main() {
    std::unordered_map<std::string, long long> m;
    for (int i = 0; i < 50000; i++) {
        m[std::to_string(i)] = (long long)i * 2;
    }
    long long total = 0;
    for (int j = 0; j < 50000; j++) {
        total += m[std::to_string(j)];
    }
    printf("%lld\n", total);
    return 0;
}
