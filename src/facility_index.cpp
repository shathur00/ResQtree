#include "facility_index.h"

#include <algorithm>

bool FacilityIndex::add(const Facility& facility) {
    if (byId_.count(facility.id) != 0) return false;
    const std::string key = normalizeType(facility.type);
    Facility stored = facility;
    stored.type = key;
    trees_[key].insert(stored);
    byId_[facility.id] = stored;
    return true;
}

void FacilityIndex::build(const std::vector<Facility>& facilities) {
    trees_.clear();
    byId_.clear();
    std::unordered_map<std::string, std::vector<Facility>> buckets;
    for (const Facility& facility : facilities) {
        if (byId_.count(facility.id) != 0) {
            // skip duplicate id (should already be unique in loaded data)
            continue;
        }
        const std::string key = normalizeType(facility.type);
        Facility stored = facility;
        stored.type = key;
        buckets[key].push_back(stored);
    }
    for (auto& entry : buckets) {
        // insert ids into byId
        for (const auto& f : entry.second) byId_[f.id] = f;
        trees_[entry.first].build(entry.second);
    }
}

bool FacilityIndex::remove(int id) {
    const auto it = byId_.find(id);
    if (it == byId_.end()) return false;
    const std::string type = it->second.type;
    const Facility f = it->second;
    auto tit = trees_.find(type);
    if (tit != trees_.end()) {
        tit->second.remove(f);
        if (tit->second.empty()) trees_.erase(tit);
    }
    byId_.erase(it);
    return true;
}

std::optional<Facility> FacilityIndex::findById(int id) const {
    const auto it = byId_.find(id);
    if (it == byId_.end()) return std::nullopt;
    return it->second;
}

FacilityIndex::NearestResult FacilityIndex::nearestInType(const std::string& type, double lat, double lon,
                                                          std::size_t* nodesVisited) const {
    const auto it = trees_.find(normalizeType(type));
    if (it == trees_.end()) {
        if (nodesVisited) *nodesVisited = 0;
        return std::nullopt;
    }
    std::size_t nv = 0;
    auto r = it->second.nearest(lat, lon, nodesVisited ? &nv : nullptr);
    if (nodesVisited) *nodesVisited = nv;
    return r;
}

FacilityIndex::NearestResult FacilityIndex::nearestOverall(double lat, double lon,
                                                          std::size_t* nodesVisited) const {
    return nearestInTypes({}, lat, lon, nodesVisited);
}

FacilityIndex::NearestResult FacilityIndex::nearestInTypes(const std::vector<std::string>& types, double lat, double lon,
                                                          std::size_t* nodesVisited) const {
    NearestResult best = std::nullopt;
    std::size_t totalNv = 0;
    auto searchType = [&](const std::string& t) {
        const auto it = trees_.find(normalizeType(t));
        if (it == trees_.end()) return;
        std::size_t nv = 0;
        const auto cand = it->second.nearest(lat, lon, nodesVisited ? &nv : nullptr);
        totalNv += nv;
        if (cand.has_value() && (!best.has_value() || cand->second < best->second)) best = cand;
    };
    if (types.empty()) {
        for (const auto& e : trees_) searchType(e.first);
    } else {
        for (const auto& t : types) searchType(t);
    }
    if (nodesVisited) *nodesVisited = totalNv;
    return best;
}

std::vector<std::pair<Facility, double>> FacilityIndex::kNearestInType(const std::string& type, double lat, double lon,
                                                                      std::size_t k, std::size_t* nodesVisited) const {
    const auto it = trees_.find(normalizeType(type));
    if (it == trees_.end()) {
        if (nodesVisited) *nodesVisited = 0;
        return {};
    }
    std::size_t nv = 0;
    auto r = it->second.kNearest(lat, lon, k, nodesVisited ? &nv : nullptr);
    if (nodesVisited) *nodesVisited = nv;
    return r;
}

std::vector<std::pair<Facility, double>> FacilityIndex::kNearestOverall(double lat, double lon, std::size_t k,
                                                                       std::size_t* nodesVisited) const {
    return kNearestInTypes({}, lat, lon, k, nodesVisited);
}

std::vector<std::pair<Facility, double>> FacilityIndex::kNearestInTypes(const std::vector<std::string>& types, double lat, double lon,
                                                                       std::size_t k, std::size_t* nodesVisited) const {
    // take up to k from each tree, then best k overall
    std::vector<std::pair<Facility, double>> merged;
    std::size_t totalNv = 0;
    auto searchType = [&](const std::string& t) {
        const auto it = trees_.find(normalizeType(t));
        if (it == trees_.end()) return;
        std::size_t nv = 0;
        auto v = it->second.kNearest(lat, lon, k, nodesVisited ? &nv : nullptr);
        totalNv += nv;
        for (auto& x : v) merged.push_back(x);
    };
    if (types.empty()) for (const auto& e : trees_) searchType(e.first);
    else for (const auto& t : types) searchType(t);

    std::sort(merged.begin(), merged.end(), [](const auto& a, const auto& b){ return a.second < b.second; });
    if (merged.size() > k) merged.resize(k);
    if (nodesVisited) *nodesVisited = totalNv;
    return merged;
}

std::vector<std::pair<Facility, double>> FacilityIndex::radiusInType(const std::string& type, double lat, double lon, double radiusKm,
                                                                   std::size_t* nodesVisited) const {
    const auto it = trees_.find(normalizeType(type));
    if (it == trees_.end()) {
        if (nodesVisited) *nodesVisited = 0;
        return {};
    }
    std::size_t nv = 0;
    auto r = it->second.radiusSearch(lat, lon, radiusKm, nodesVisited ? &nv : nullptr);
    if (nodesVisited) *nodesVisited = nv;
    return r;
}

std::vector<std::pair<Facility, double>> FacilityIndex::radiusOverall(double lat, double lon, double radiusKm,
                                                                     std::size_t* nodesVisited) const {
    return radiusInTypes({}, lat, lon, radiusKm, nodesVisited);
}

std::vector<std::pair<Facility, double>> FacilityIndex::radiusInTypes(const std::vector<std::string>& types, double lat, double lon, double radiusKm,
                                                                     std::size_t* nodesVisited) const {
    std::vector<std::pair<Facility, double>> merged;
    std::size_t totalNv = 0;
    auto searchType = [&](const std::string& t) {
        const auto it = trees_.find(normalizeType(t));
        if (it == trees_.end()) return;
        std::size_t nv = 0;
        auto v = it->second.radiusSearch(lat, lon, radiusKm, nodesVisited ? &nv : nullptr);
        totalNv += nv;
        for (auto& x : v) merged.push_back(x);
    };
    if (types.empty()) for (const auto& e : trees_) searchType(e.first);
    else for (const auto& t : types) searchType(t);
    std::sort(merged.begin(), merged.end(), [](const auto& a, const auto& b){ return a.second < b.second; });
    if (nodesVisited) *nodesVisited = totalNv;
    return merged;
}

std::vector<std::string> FacilityIndex::types() const {
    std::vector<std::string> names;
    for (const auto& e : trees_) names.push_back(e.first);
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<FacilityIndex::TypeStat> FacilityIndex::typeStats() const {
    std::vector<TypeStat> out;
    for (const auto& e : trees_) {
        TypeStat s;
        s.name = e.first;
        s.count = e.second.size();
        s.height = e.second.height();
        out.push_back(s);
    }
    std::sort(out.begin(), out.end(), [](const TypeStat&a, const TypeStat&b){ return a.name < b.name; });
    return out;
}

std::size_t FacilityIndex::countOfType(const std::string& type) const {
    const auto it = trees_.find(normalizeType(type));
    if (it == trees_.end()) return 0;
    return it->second.size();
}

int FacilityIndex::heightOfType(const std::string& type) const {
    const auto it = trees_.find(normalizeType(type));
    if (it == trees_.end()) return 0;
    return it->second.height();
}

std::size_t FacilityIndex::size() const {
    std::size_t total = 0;
    for (const auto& e : trees_) total += e.second.size();
    return total;
}
bool FacilityIndex::empty() const { return size() == 0; }
void FacilityIndex::clear() { trees_.clear(); byId_.clear(); }
void FacilityIndex::rebuildAll() {
    for (auto& e : trees_) e.second.rebuild();
}