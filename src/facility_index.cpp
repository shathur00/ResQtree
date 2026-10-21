// facility_index.cpp
//
// Implementation of the per-type index declared in facility_index.h.

#include "facility_index.h"

#include <algorithm>

FacilityIndex::NearestResult FacilityIndex::nearestInType(const std::string& type,
                                                           double lat, double lon) const {
    // Types are stored lowercase, so normalise the query the same way.
    const std::string key = normalizeType(type);

    const auto it = trees_.find(key);
    if (it == trees_.end()) {
        return std::nullopt;
    }

    return it->second.nearest(lat, lon);
}

FacilityIndex::NearestResult FacilityIndex::nearestOverall(double lat, double lon) const {
    NearestResult best = std::nullopt;

    // Ask each type's tree for its own nearest facility and keep the best
    // of those answers. The number of types is small compared to the
    // number of facilities, so this stays fast.
    for (const auto& entry : trees_) {
        const NearestResult candidate = entry.second.nearest(lat, lon);
        if (candidate.has_value() && (!best.has_value() || candidate->second < best->second)) {
            best = candidate;
        }
    }

    return best;
}

void FacilityIndex::add(const Facility& facility) {
    // Make sure the type is lowercase before it becomes a map key.
    const std::string key = normalizeType(facility.type);

    // Store a copy whose type is normalised too, so that every facility
    // held in a tree carries the same lowercase spelling.
    Facility stored = facility;
    stored.type = key;

    // operator[] creates an empty tree for a type we have not seen yet,
    // and gives us a reference to the tree for a type we have.
    trees_[key].insert(stored);
}

void FacilityIndex::build(const std::vector<Facility>& facilities) {
    // Start from scratch.
    trees_.clear();

    // Bucket the facilities by type first. Doing it in two passes lets us
    // build each tree in one go, which keeps every tree balanced.
    std::unordered_map<std::string, std::vector<Facility>> buckets;

    for (const Facility& facility : facilities) {
        const std::string key = normalizeType(facility.type);

        // Store a copy whose type is normalised too, so that every
        // facility held in a tree carries the same lowercase spelling.
        Facility stored = facility;
        stored.type = key;

        buckets[key].push_back(stored);
    }

    // Now build one KD-Tree per bucket.
    for (auto& entry : buckets) {
        trees_[entry.first].build(entry.second);
    }
}

std::vector<std::string> FacilityIndex::types() const {
    std::vector<std::string> names;
    names.reserve(trees_.size());

    for (const auto& entry : trees_) {
        names.push_back(entry.first);
    }

    // Alphabetical order makes the menu easy to read.
    std::sort(names.begin(), names.end());
    return names;
}

std::size_t FacilityIndex::countOfType(const std::string& type) const {
    const auto it = trees_.find(normalizeType(type));
    if (it == trees_.end()) {
        return 0;
    }
    return it->second.size();
}

std::size_t FacilityIndex::size() const {
    std::size_t total = 0;
    for (const auto& entry : trees_) {
        total += entry.second.size();
    }
    return total;
}

bool FacilityIndex::empty() const {
    return size() == 0;
}

void FacilityIndex::clear() {
    trees_.clear();
}
