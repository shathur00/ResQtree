#ifndef RESQTREE_FACILITY_INDEX_H
#define RESQTREE_FACILITY_INDEX_H

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "facility.h"
#include "kdtree.h"

class FacilityIndex {
public:
    using NearestResult = std::optional<std::pair<Facility, double>>;

    struct TypeStat {
        std::string name;
        std::size_t count = 0;
        int height = 0;
    };

    FacilityIndex() = default;

    void build(const std::vector<Facility>& facilities);
    bool add(const Facility& facility);
    bool remove(int id);
    std::optional<Facility> findById(int id) const;

    NearestResult nearestInType(const std::string& type, double lat, double lon,
                               std::size_t* nodesVisited = nullptr) const;
    NearestResult nearestOverall(double lat, double lon, std::size_t* nodesVisited = nullptr) const;
    NearestResult nearestInTypes(const std::vector<std::string>& types, double lat, double lon,
                                std::size_t* nodesVisited = nullptr) const;

    std::vector<std::pair<Facility, double>> kNearestInType(const std::string& type,
                                                          double lat, double lon, std::size_t k,
                                                          std::size_t* nodesVisited = nullptr) const;
    std::vector<std::pair<Facility, double>> kNearestOverall(double lat, double lon, std::size_t k,
                                                             std::size_t* nodesVisited = nullptr) const;
    std::vector<std::pair<Facility, double>> kNearestInTypes(const std::vector<std::string>& types,
                                                             double lat, double lon, std::size_t k,
                                                             std::size_t* nodesVisited = nullptr) const;

    std::vector<std::pair<Facility, double>> radiusInType(const std::string& type,
                                                          double lat, double lon, double radiusKm,
                                                          std::size_t* nodesVisited = nullptr) const;
    std::vector<std::pair<Facility, double>> radiusOverall(double lat, double lon, double radiusKm,
                                                           std::size_t* nodesVisited = nullptr) const;
    std::vector<std::pair<Facility, double>> radiusInTypes(const std::vector<std::string>& types,
                                                           double lat, double lon, double radiusKm,
                                                           std::size_t* nodesVisited = nullptr) const;

    std::vector<std::string> types() const;
    std::vector<TypeStat> typeStats() const;
    std::size_t countOfType(const std::string& type) const;
    int heightOfType(const std::string& type) const;
    std::size_t size() const;
    bool empty() const;
    void clear();
    void rebuildAll();

private:
    std::unordered_map<std::string, KDTree> trees_;
    std::unordered_map<int, Facility> byId_;
};

#endif