#include "bruteforce.h"

#include <algorithm>

namespace brute {
std::optional<std::pair<Facility, double>> bruteNearest(const std::vector<Facility>& facilities, double lat, double lon) {
    std::optional<std::pair<Facility,double>> best;
    for (const auto& f : facilities) {
        double d = haversine::haversineKm(lat,lon,f.lat,f.lon);
        if (!best.has_value() || d < best->second) best = std::make_pair(f,d);
    }
    return best;
}
std::vector<std::pair<Facility, double>> bruteKNearest(const std::vector<Facility>& facilities, double lat, double lon, std::size_t k) {
    std::vector<std::pair<Facility,double>> v;
    for (const auto& f : facilities) v.push_back({f, haversine::haversineKm(lat,lon,f.lat,f.lon)});
    std::sort(v.begin(), v.end(), [](const auto&a,const auto&b){return a.second<b.second;});
    if (k < v.size()) v.resize(k);
    return v;
}
std::vector<std::pair<Facility, double>> bruteRadius(const std::vector<Facility>& facilities, double lat, double lon, double radiusKm) {
    std::vector<std::pair<Facility,double>> v;
    if (radiusKm < 0) return v;
    for (const auto& f : facilities) {
        double d = haversine::haversineKm(lat,lon,f.lat,f.lon);
        if (d <= radiusKm) v.push_back({f,d});
    }
    std::sort(v.begin(), v.end(), [](const auto&a,const auto&b){return a.second<b.second;});
    return v;
}
}