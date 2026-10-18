// Monte Carlo simulation ( no pi )

// Goal to beat
// 
// use the "time" command to measure the execution time of the program
//
// mcnp.cpp time (compiled with g++ at -O3) output:
//real    0m1.908s
//user    0m1.906s
//sys     0m0.000s
//
// CPU: AMD Ryzen 7 7800X 8-Core Processor
// GPU: AMD Radeon RX 9070 XT 16GB
//
// if you cant beat it by atleast 50% then you are not allowed to submit your solution

package benchmarks;

class MonteCarloNoPi {
    static long random(long seed) {
        return (seed * 1664525L + 1013904223L) % 4294967296L;
    }

    public static void main(String[] args) {
        long iterations = 1000000000L;
        double threshold = 0.75;
        long seed = 123456789L;
        long risky = 0L;

        System.out.println("Starting Monte Carlo simulation...");
        System.out.println("Iterations: " + iterations);

        long i = 0L;
        while (i < iterations) {
            long randomValue = random(seed);
            seed = randomValue;

            double normalized = randomValue / 4294967296.0;

            // Simulate a risk event.
            // Combine two independent random values.
            long randomValue2 = random(seed);
            seed = randomValue2;

            double normalized2 = randomValue2 / 4294967296.0;

            double score = normalized * normalized2;

            if (score > threshold) {
                risky += 1L;
            }

            i += 1L;
        }

        double probability = (double) risky / iterations;

        System.out.println("Done!");
        System.out.println("Risky outcomes: " + risky);
        System.out.println("Estimated probability: " + probability);
    }
}
