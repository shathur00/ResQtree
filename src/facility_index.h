// facility_index.h
//
// A thin layer on top of KDTree that keeps ONE SEPARATE TREE PER FACILITY
// TYPE.
//
// Why not one big tree for everything? Because a search for "the nearest
// hospital" should not have to wade through thousands of bus stops to find
// one. Splitting the data by type means each tree only holds the points
// that are relevant to the query, which makes every search smaller and
// faster.
//
// The types live in a std::unordered_map<std::string, KDTree>, so looking
// up the tree for a type is O(1) on average.
//
// The layer is thin on purpose: the KD-Tree does the real work, and this
// class just organises the trees. That keeps the door open for the later
// versions (k-nearest, radius search, delete) to be added here without
// touching the tree implementation.
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
    // The result of a nearest-neighbour query.
    using NearestResult = std::optional<std::pair<Facility, double>>;

    FacilityIndex() = default;

    // Group the given facilities by type and build one KD-Tree per type.
    // Any previously loaded data is discarded.
    void build(const std::vector<Facility>& facilities);

    // Add a single facility to the tree of its own type, creating that
    // tree if it does not exist yet.
    void add(const Facility& facility);

    // Nearest facility of the given type. The type string is normalised
    // to lowercase first, so "Hospital" works as well as "hospital".
    // Returns std::nullopt when the type is unknown or the tree is empty.
    NearestResult nearestInType(const std::string& type, double lat, double lon) const;

    // Nearest facility of ANY type. The type of the returned facility
    // says which group it came from.
    NearestResult nearestOverall(double lat, double lon) const;

    // All the type names currently present, sorted alphabetically.
    std::vector<std::string> types() const;

    // How many facilities of the given type are stored (0 if unknown).
    std::size_t countOfType(const std::string& type) const;

    // Total number of facilities across all types.
    std::size_t size() const;

    // True when no facilities are stored at all.
    bool empty() const;

    // Drop everything.
    void clear();

private:
    // One KD-Tree per type. The key is the lowercased type name.
    std::unordered_map<std::string, KDTree> trees_;
};

#endif  // RESQTREE_FACILITY_INDEX_H
