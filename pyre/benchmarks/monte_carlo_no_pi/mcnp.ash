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

local ITERATIONS = 1000000000;
local threshold = 0.75;

forge random(seed) {
    seed = (seed * 1664525 + 1013904223) % 4294967296;
    yield seed;
}

local seed = 123456789;
local risky = 0;

say "Starting Monte Carlo simulation...";
say "Iterations: " + str(ITERATIONS);

local i = 0;

during i < ITERATIONS {
    local random_value = random(seed);
    seed = random_value;

    local normalized = random_value / 4294967296.0;

    // Simulate a risk event.
    // Combine two independent random values.
    local random_value_2 = random(seed);
    seed = random_value_2;

    local normalized_2 = random_value_2 / 4294967296.0;

    local score = normalized * normalized_2;

    given score > threshold {
        risky += 1;
    }

    i += 1;
}

local probability = risky / ITERATIONS;

say "Done!";
say "Risky outcomes: " + str(risky);
say "Estimated probability: " + str(probability);