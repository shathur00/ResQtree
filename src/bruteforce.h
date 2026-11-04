#ifndef RESQTREE_BRUTEFORCE_H
#define RESQTREE_BRUTEFORCE_H

#include <optional>
#include <utility>
#include <vector>

#include "facility.h"
#include "haversine.h"

namespace brute {
std::optional<std::pair<Facility, double>> bruteNearest(const std::vector<Facility>& facilities,
                                                        double lat, double lon);
std::vector<std::pair<Facility, double>> bruteKNearest(const std::vector<Facility>& facilities,
                                                       double lat, double lon, std::size_t k);
std::vector<std::pair<Facility, double>> bruteRadius(const std::vector<Facility>& facilities,
                                                     double lat, double lon, double radiusKm);
}
#endif