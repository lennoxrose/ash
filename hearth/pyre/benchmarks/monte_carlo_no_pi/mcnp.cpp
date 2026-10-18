#include <iostream>
#include <cstdint>

#define ITERATIONS 1000000000ULL

uint32_t random(uint32_t& seed)
{
    seed = seed * 1664525u + 1013904223u;
    return seed;
}

int main()
{
    const double threshold = 0.75;

    uint32_t seed = 123456789;
    uint64_t risky = 0;

    std::cout << "Starting Monte Carlo simulation...\n";
    std::cout << "Iterations: " << ITERATIONS << "\n";

    for (uint64_t i = 0; i < ITERATIONS; i++) {
        uint32_t random_value = random(seed);
        double normalized =
            static_cast<double>(random_value) / 4294967296.0;

        uint32_t random_value_2 = random(seed);
        double normalized_2 =
            static_cast<double>(random_value_2) / 4294967296.0;

        double score = normalized * normalized_2;

        if (score > threshold) {
            risky++;
        }
    }

    double probability =
        static_cast<double>(risky) / static_cast<double>(ITERATIONS);

    std::cout << "Done!\n";
    std::cout << "Risky outcomes: " << risky << "\n";
    std::cout << "Estimated probability: " << probability << "\n";

    return 0;
}