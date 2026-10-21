// facility.h
//
// The Facility struct: one row of the dataset is one searchable point.
//
// A "facility" is anything we might need in an emergency: a hospital, a
// blood bank, an ambulance depot, a bus stop, a pharmacy, and so on.
// Every facility has:
//   - id    : unique integer id (taken from the CSV file)
//   - name  : human readable name, e.g. "City General Hospital"
//   - type  : the category, ALWAYS stored in lowercase, e.g. "hospital"
//   - lat   : latitude  in degrees (north positive)
//   - lon   : longitude in degrees (east positive)
//
// The header is header-only and free of any dependency, so every other
// file in the project can include it cheaply.
#ifndef RESQTREE_FACILITY_H
#define RESQTREE_FACILITY_H

#include <algorithm>
#include <cctype>
#include <string>

struct Facility {
    int id = 0;
    std::string name;
    std::string type;
    double lat = 0.0;
    double lon = 0.0;
};

// Remove whitespace from both ends of a string.
// CSV files very often have stray spaces like " hospital" after a comma.
inline std::string trim(const std::string& s) {
    size_t begin = 0;
    size_t end = s.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
        --end;
    }
    return s.substr(begin, end - begin);
}

// Convert an ASCII string to lowercase.
inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

// Every type string in the program goes through this function: the CSV
// loader calls it on each row, and the menu calls it on whatever the user
// typed. That way "Hospital", "HOSPITAL" and " hospital " all end up in
// the same bucket of the facility index.
inline std::string normalizeType(const std::string& raw) {
    return toLower(trim(raw));
}

#endif  // RESQTREE_FACILITY_H
