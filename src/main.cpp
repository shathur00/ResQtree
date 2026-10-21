// main.cpp
//
// The command-line menu of ResQTree.
//
// The program loads the facility data set, splits it into one KD-Tree per
// facility type, and then lets the user search for the nearest facility
// of a given type (or of any type) from a given location.
//
// Later versions can add more menu entries (k-nearest, radius search, ...)
// without touching the KD-Tree, because all the searching logic lives in
// KDTree and FacilityIndex.

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "csv_loader.h"
#include "facility.h"
#include "facility_index.h"
#include "kdtree.h"

namespace {

// The location of the sample data set. The program is usually started
// from the project root, so "data/facilities.csv" is the first thing to
// try; "../data/facilities.csv" covers the case where it was started
// from inside the build folder.
const char* const kDefaultCsvPaths[] = {"data/facilities.csv", "../data/facilities.csv"};

// Try the default paths in order and use the first one that exists.
std::string findDefaultCsv() {
    for (const char* path : kDefaultCsvPaths) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return kDefaultCsvPaths[0];  // report the primary path in the error
}

// Read a number from the user. Returns false if the input was not a
// valid number, so the caller can ask again.
bool readDouble(const std::string& prompt, double& out) {
    std::string line;
    std::cout << prompt;
    if (!std::getline(std::cin, line)) {
        return false;
    }
    // Trim first, so that " 11.5 " is accepted.
    const std::string text = trim(line);
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        // Every character of the text has to be part of the number.
        if (consumed != text.size()) {
            return false;
        }
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Read a non-empty line from the user.
bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt;
    if (!std::getline(std::cin, out)) {
        return false;
    }
    out = trim(out);
    return !out.empty();
}

// Print one search result in a readable block.
void printResult(const KDTree::NearestResult& result, double lat, double lon) {
    if (!result.has_value()) {
        std::cout << "  No facility found.\n";
        return;
    }

    const Facility& facility = result->first;
    const double distance = result->second;

    std::cout << "  Nearest facility\n";
    std::cout << "  ----------------\n";
    std::cout << "  Name     : " << facility.name << "\n";
    std::cout << "  Type     : " << facility.type << "\n";
    std::cout << "  ID       : " << facility.id << "\n";
    std::cout << "  Location : " << facility.lat << ", " << facility.lon << "\n";
    std::cout << "  Distance : " << distance << " km\n";
    std::cout << "  From     : " << lat << ", " << lon << "\n";
}

// The search menu. Returns when the user chooses to go back.
void searchMenu(const FacilityIndex& index) {
    std::cout << "\n--- Search ---\n";
    std::cout << "1. Nearest facility of a given type\n";
    std::cout << "2. Nearest facility of any type\n";
    std::cout << "3. List the available types\n";
    std::cout << "0. Back to main menu\n";

    std::string choice;
    if (!readLine("Your choice: ", choice)) {
        return;
    }

    if (choice == "0") {
        return;
    }

    if (choice == "3") {
        const std::vector<std::string> types = index.types();
        if (types.empty()) {
            std::cout << "  No types loaded.\n";
            return;
        }
        std::cout << "  Available types:\n";
        for (const std::string& type : types) {
            std::cout << "    - " << type << " (" << index.countOfType(type) << " facilities)\n";
        }
        return;
    }

    if (choice != "1" && choice != "2") {
        std::cout << "  Invalid choice.\n";
        return;
    }

    // Both search options need a location to search from.
    double lat = 0.0;
    double lon = 0.0;
    if (!readDouble("Latitude  (e.g. 11.0168): ", lat)) {
        std::cout << "  Invalid latitude.\n";
        return;
    }
    if (!readDouble("Longitude (e.g. 76.9558): ", lon)) {
        std::cout << "  Invalid longitude.\n";
        return;
    }

    if (choice == "1") {
        std::string type;
        if (!readLine("Facility type (e.g. hospital, blank for any): ", type)) {
            // A blank line means "any type". readLine() reports an empty
            // line as false, so we treat that as the any-type search.
            std::cout << "\n";
            printResult(index.nearestOverall(lat, lon), lat, lon);
            return;
        }
        const std::string key = normalizeType(type);
        if (index.countOfType(key) == 0) {
            std::cout << "  Unknown type \"" << key << "\". Use option 3 to list them.\n";
            return;
        }
        std::cout << "\n";
        printResult(index.nearestInType(key, lat, lon), lat, lon);
        return;
    }

    std::cout << "\n";
    printResult(index.nearestOverall(lat, lon), lat, lon);
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << "===== ResQTree =====\n";
    std::cout << "Nearest-facility search with KD-Trees.\n\n";

    // An optional first argument overrides the data set location.
    const std::string csvPath = (argc > 1) ? std::string(argv[1]) : findDefaultCsv();

    std::cout << "Loading data from: " << csvPath << "\n";
    const csv_loader::LoadResult loaded = csv_loader::load(csvPath);
    if (!loaded.ok) {
        std::cerr << "Error: " << loaded.error << "\n";
        std::cerr << "Usage: " << argv[0] << " [path/to/facilities.csv]\n";
        return 1;
    }

    if (loaded.facilities.empty()) {
        std::cerr << "Error: no usable facilities found in " << csvPath << "\n";
        return 1;
    }

    // Group the facilities by type and build one KD-Tree per type.
    FacilityIndex index;
    index.build(loaded.facilities);

    std::cout << "Loaded " << index.size() << " facilities across "
              << index.types().size() << " types.\n";
    if (loaded.skipped > 0) {
        std::cout << "Skipped " << loaded.skipped << " malformed row(s), first at line "
                  << loaded.firstBadLine << ".\n";
    }

    bool running = true;
    while (running) {
        std::cout << "\n===== Main Menu =====\n";
        std::cout << "1. Nearest facility search\n";
        std::cout << "2. Show data set summary\n";
        std::cout << "0. Exit\n";

        std::string choice;
        if (!readLine("Your choice: ", choice)) {
            break;
        }

        if (choice == "0") {
            running = false;
        } else if (choice == "1") {
            searchMenu(index);
        } else if (choice == "2") {
            std::cout << "\n--- Data set ---\n";
            std::cout << "Source : " << csvPath << "\n";
            std::cout << "Total  : " << index.size() << " facilities\n";
            for (const std::string& type : index.types()) {
                std::cout << "  " << type << " : " << index.countOfType(type) << "\n";
            }
        } else {
            std::cout << "  Invalid choice.\n";
        }
    }

    std::cout << "\nGoodbye.\n";
    return 0;
}
