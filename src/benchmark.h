#ifndef RESQTREE_BENCHMARK_H
#define RESQTREE_BENCHMARK_H

#include <cstddef>
#include <string>
#include <vector>

struct BenchOptions {
    std::vector<std::size_t> sizes = {1000, 10000, 100000, 1000000};
    std::size_t queries = 1000;
    unsigned seed = 42;
};

void runBenchmark(const BenchOptions& opts);

#endif