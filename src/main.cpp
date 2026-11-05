// main.cpp
//
// Command-line interface for ResQTree: a KD-Tree based nearest-emergency-
// facility finder. Reads the (optional) CSV path given on the command line
// (defaulting to data/facilities.csv / ../data/facilities.csv), builds one
// KD-Tree per facility type inside a FacilityIndex, then runs an
// interactive menu loop.

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "csv_loader.h"
#include "facility.h"
#include "facility_index.h"
#include "haversine.h"

namespace fs = std::filesystem;

namespace {
const char* const kDefaultCsvPaths[] = {"data/facilities.csv", "../data/facilities.csv"};

std::string findDefaultCsv() {
    for (const char* p : kDefaultCsvPaths) if (fs::exists(p)) return p;
    return kDefaultCsvPaths[0];
}

bool readDouble(const std::string& prompt, double& out) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    const std::string text = trim(line);
    if (text.empty()) return false;
    try {
        std::size_t consumed = 0;
        const double v = std::stod(text, &consumed);
        if (consumed != text.size()) return false;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool readInt(const std::string& prompt, int& out) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    const std::string text = trim(line);
    if (text.empty()) return false;
    try {
        std::size_t consumed = 0;
        const long v = std::stol(text, &consumed);
        if (consumed != text.size()) return false;
        out = static_cast<int>(v);
        return true;
    } catch (...) {
        return false;
    }
}

bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt;
    if (!std::getline(std::cin, out)) return false;
    out = trim(out);
    return true;
}

std::vector<std::string> splitTypes(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = normalizeType(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

void printTypes(const FacilityIndex& index) {
    const auto stats = index.typeStats();
    if (stats.empty()) {
        std::cout << "  (no types)\n";
        return;
    }
    for (const auto& s : stats) {
        std::cout << "  " << s.name << " : " << s.count << " facilities, tree height "
                  << s.height << "\n";
    }
}

bool validLat(double lat) { return lat >= -90.0 && lat <= 90.0; }
bool validLon(double lon) { return lon >= -180.0 && lon <= 180.0; }

void printTable(const std::vector<std::pair<Facility, double>>& res) {
    if (res.empty()) {
        std::cout << "  (no facilities)\n";
        return;
    }
    std::cout << "rank  name                          type             id    distance (km)\n";
    int rank = 1;
    for (const auto& r : res) {
        std::cout << rank++ << "     " << r.first.name << "  " << r.first.type << "  "
                  << r.first.id << "  ";
        std::cout << std::fixed;
        std::cout.precision(2);
        std::cout << r.second << "\n";
    }
}
}  // namespace

int main(int argc, char* argv[]) {
    std::cout << "===== ResQTree =====\n";
    const std::string csvPath = (argc > 1) ? std::string(argv[1]) : findDefaultCsv();
    std::cout << "Loading data from: " << csvPath << "\n";

    const csv_loader::LoadResult loaded = csv_loader::load(csvPath);
    if (!loaded.ok) {
        std::cerr << "Error: " << loaded.error << "\n";
        return 1;
    }
    if (loaded.facilities.empty()) {
        std::cerr << "Error: no usable facilities loaded.\n";
        return 1;
    }

    FacilityIndex index;
    index.build(loaded.facilities);

    std::cout << "Loaded " << index.size() << " facilities across "
              << index.types().size() << " types.\n";
    if (loaded.skipped > 0) {
        std::cout << "Skipped " << loaded.skipped << " row(s); first bad row at line "
                  << loaded.firstBadLine << ".\n";
    }

    bool running = true;
    while (running) {
        std::cout << "\n===== Main Menu =====\n";
        std::cout << "1. Find nearest facility\n";
        std::cout << "2. Find k nearest facilities\n";
        std::cout << "3. Find facilities within a radius\n";
        std::cout << "4. Insert a facility\n";
        std::cout << "5. Delete a facility (by id)\n";
        std::cout << "6. Look up a facility by id\n";
        std::cout << "7. List facility types and tree stats\n";
        std::cout << "8. Rebalance all trees\n";
        std::cout << "9. Exit\n";

        std::string choice;
        if (!readLine("Your choice: ", choice)) {
            std::cout << "\nEOF on stdin. Exiting.\n";
            break;
        }

        if (choice == "9" || choice == "0") {
            running = false;
            continue;
        }
        if (choice == "1") {
            std::cout << "Available types:\n";
            printTypes(index);
            std::string typeSpec;
            if (!readLine("Type(s) (e.g. hospital or hospital,blood_bank or all): ", typeSpec)) {
                std::cout << "\n"; continue;
            }
            std::vector<std::string> types;
            if (normalizeType(typeSpec) == "all") types = index.types();
            else types = splitTypes(typeSpec);

            std::vector<std::string> unknown, valid;
            for (const auto& t : types) {
                if (index.countOfType(t) == 0) unknown.push_back(t);
                else valid.push_back(t);
            }
            if (!unknown.empty()) {
                std::cout << "Unknown type(s): ";
                for (const auto& u : unknown) std::cout << u << " ";
                std::cout << "\nAvailable types: ";
                for (const auto& t : index.types()) std::cout << t << " ";
                std::cout << "\n";
            }
            if (valid.empty()) { std::cout << "No valid types.\n"; continue; }

            double lat, lon;
            if (!readDouble("Latitude: ", lat) || !validLat(lat)) {
                std::cout << "  Invalid latitude.\n"; continue;
            }
            if (!readDouble("Longitude: ", lon) || !validLon(lon)) {
                std::cout << "  Invalid longitude.\n"; continue;
            }
            const auto res = index.nearestInTypes(valid, lat, lon);
            if (!res.has_value()) std::cout << "No facility found.\n";
            else {
                std::cout << "rank  name                          type             id    distance (km)\n";
                std::cout << "1     " << res->first.name << "  " << res->first.type << "  "
                          << res->first.id << "  ";
                std::cout << std::fixed; std::cout.precision(2);
                std::cout << res->second << "\n";
            }
        } else if (choice == "2") {
            std::cout << "Available types:\n";
            printTypes(index);
            std::string typeSpec;
            if (!readLine("Type(s): ", typeSpec)) { std::cout << "\n"; continue; }
            std::vector<std::string> types;
            if (normalizeType(typeSpec) == "all") types = index.types();
            else types = splitTypes(typeSpec);
            std::vector<std::string> unknown, valid;
            for (const auto& t : types) {
                if (index.countOfType(t) == 0) unknown.push_back(t);
                else valid.push_back(t);
            }
            if (!unknown.empty()) {
                std::cout << "Unknown type(s): ";
                for (const auto& u : unknown) std::cout << u << " ";
                std::cout << "\nAvailable types: ";
                for (const auto& t : index.types()) std::cout << t << " ";
                std::cout << "\n";
            }
            if (valid.empty()) { std::cout << "No valid types.\n"; continue; }
            double lat, lon; int k;
            if (!readDouble("Latitude: ", lat) || !validLat(lat)) { std::cout << "  Invalid latitude.\n"; continue; }
            if (!readDouble("Longitude: ", lon) || !validLon(lon)) { std::cout << "  Invalid longitude.\n"; continue; }
            if (!readInt("k: ", k) || k < 1) { std::cout << "  Invalid k.\n"; continue; }
            const auto res = index.kNearestInTypes(valid, lat, lon, (std::size_t)k);
            printTable(res);
        } else if (choice == "3") {
            std::cout << "Available types:\n";
            printTypes(index);
            std::string typeSpec;
            if (!readLine("Type(s): ", typeSpec)) { std::cout << "\n"; continue; }
            std::vector<std::string> types;
            if (normalizeType(typeSpec) == "all") types = index.types();
            else types = splitTypes(typeSpec);
            std::vector<std::string> unknown, valid;
            for (const auto& t : types) {
                if (index.countOfType(t) == 0) unknown.push_back(t);
                else valid.push_back(t);
            }
            if (!unknown.empty()) {
                std::cout << "Unknown type(s): ";
                for (const auto& u : unknown) std::cout << u << " ";
                std::cout << "\nAvailable types: ";
                for (const auto& t : index.types()) std::cout << t << " ";
                std::cout << "\n";
            }
            if (valid.empty()) { std::cout << "No valid types.\n"; continue; }
            double lat, lon, r;
            if (!readDouble("Latitude: ", lat) || !validLat(lat)) { std::cout << "  Invalid latitude.\n"; continue; }
            if (!readDouble("Longitude: ", lon) || !validLon(lon)) { std::cout << "  Invalid longitude.\n"; continue; }
            if (!readDouble("Radius (km): ", r) || r <= 0) { std::cout << "  Invalid radius.\n"; continue; }
            const auto res = index.radiusInTypes(valid, lat, lon, r);
            printTable(res);
        } else if (choice == "4") {
            int id;
            if (!readInt("Id: ", id)) { std::cout << "  Invalid id.\n"; continue; }
            if (index.findById(id).has_value()) { std::cout << "  Duplicate id; rejected.\n"; continue; }
            std::string name, type;
            if (!readLine("Name: ", name)) { std::cout << "\n"; continue; }
            if (!readLine("Type: ", type)) { std::cout << "\n"; continue; }
            double lat, lon;
            if (!readDouble("Latitude: ", lat) || !validLat(lat)) { std::cout << "  Invalid latitude.\n"; continue; }
            if (!readDouble("Longitude: ", lon) || !validLon(lon)) { std::cout << "  Invalid longitude.\n"; continue; }
            Facility f{id, name, type, lat, lon};
            if (!index.add(f)) { std::cout << "  Insert failed (duplicate id).\n"; continue; }
            std::cout << "  Inserted. Total facilities: " << index.size() << "\n";
        } else if (choice == "5") {
            int id;
            if (!readInt("Id to delete: ", id)) { std::cout << "  Invalid id.\n"; continue; }
            if (index.remove(id)) std::cout << "  Deleted. Total facilities: " << index.size() << "\n";
            else std::cout << "  Not found.\n";
        } else if (choice == "6") {
            int id;
            if (!readInt("Id: ", id)) { std::cout << "  Invalid id.\n"; continue; }
            const auto f = index.findById(id);
            if (!f.has_value()) std::cout << "  Not found.\n";
            else std::cout << "  " << f->name << " | " << f->type << " | id=" << f->id
                           << " | " << f->lat << ", " << f->lon << "\n";
        } else if (choice == "7") {
            printTypes(index);
        } else if (choice == "8") {
            index.rebuildAll();
            std::cout << "  Rebalanced. Total facilities: " << index.size() << "\n";
        } else {
            std::cout << "  Invalid choice.\n";
        }
    }

    std::cout << "\nGoodbye.\n";
    return 0;
}
