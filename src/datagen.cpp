#include "datagen.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace datagen {
std::vector<Facility> generate(std::size_t n, const std::string& distribution, unsigned seed,
                               const std::string& type) {
    std::vector<Facility> out;
    out.reserve(n);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> latU(10.95, 11.10);
    std::uniform_real_distribution<double> lonU(76.90, 77.05);
    std::normal_distribution<double> normal(0.0, 1.0);

    // 20 cluster centers for clustered distribution.
    std::vector<std::pair<double,double>> centers;
    for (int i = 0; i < 20; ++i) centers.push_back({latU(rng), lonU(rng)});

    for (std::size_t i = 0; i < n; ++i) {
        double lat, lon;
        if (distribution == "clustered") {
            int c = rng() % centers.size();
            // Gaussian scatter around center, sigma ~0.005 deg.
            lat = centers[c].first + normal(rng) * 0.005;
            lon = centers[c].second + normal(rng) * 0.005;
            lat = std::min(11.10, std::max(10.95, lat));
            lon = std::min(77.05, std::max(76.90, lon));
        } else {
            lat = latU(rng);
            lon = lonU(rng);
        }
        Facility f;
        f.id = static_cast<int>(i + 1);
        f.name = "synthetic_" + std::to_string(i + 1);
        f.type = type;
        f.lat = lat;
        f.lon = lon;
        out.push_back(f);
    }
    return out;
}
}