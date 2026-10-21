// csv_loader.cpp
//
// Implementation of the CSV reader declared in csv_loader.h.

#include "csv_loader.h"

#include <fstream>
#include <sstream>

namespace csv_loader {
namespace {

// Parse a whole string into a number. Returns false if the text is not a
// complete, valid number (so "12abc" and "" are rejected).
bool parseNumber(const std::string& text, double& out) {
    const std::string trimmed = trim(text);
    if (trimmed.empty()) {
        return false;
    }
    try {
        std::size_t consumed = 0;
        const double value = std::stod(trimmed, &consumed);
        // Every character of the text has to be part of the number.
        if (consumed != trimmed.size()) {
            return false;
        }
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Parse a whole string into an int. Returns false for anything that is
// not a valid integer.
bool parseInt(const std::string& text, int& out) {
    const std::string trimmed = trim(text);
    if (trimmed.empty()) {
        return false;
    }
    try {
        std::size_t consumed = 0;
        const long value = std::stol(trimmed, &consumed);
        if (consumed != trimmed.size()) {
            return false;
        }
        out = static_cast<int>(value);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

}  // namespace

// Split a single CSV line into fields.
//
// Rules:
//   - Fields are separated by commas.
//   - A field may be wrapped in double quotes, in which case commas
//     inside the quotes belong to the field and the surrounding quotes
//     are removed.
//   - Two double quotes in a row inside a quoted field ("") mean one
//     literal double quote (the usual CSV escape).
std::vector<std::string> splitLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];

        if (inQuotes) {
            if (c == '"') {
                // Look ahead: "" inside a quoted field is one literal ".
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    current += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current += c;
            }
        } else if (c == '"') {
            inQuotes = true;
        } else if (c == ',') {
            fields.push_back(current);
            current.clear();
        } else if (c != '\r') {
            // A stray \r (Windows line endings) would break the number
            // parsing, so it is dropped here.
            current += c;
        }
    }

    fields.push_back(current);
    return fields;
}

LoadResult load(const std::string& path) {
    LoadResult result;

    std::ifstream file(path);
    if (!file) {
        result.ok = false;
        result.error = "cannot open file: " + path;
        return result;
    }

    std::string line;
    std::size_t lineNumber = 0;
    bool headerSeen = false;

    // Remember where the first malformed row was, so the caller can tell
    // the user which line of the file needs fixing.
    auto markSkipped = [&result, &lineNumber]() {
        if (result.skipped == 0) {
            result.firstBadLine = lineNumber;
        }
        ++result.skipped;
    };

    while (std::getline(file, line)) {
        ++lineNumber;

        const std::string stripped = trim(line);
        if (stripped.empty()) {
            continue;  // blank line
        }
        if (stripped[0] == '#') {
            continue;  // comment line
        }

        const std::vector<std::string> fields = splitLine(line);

        // The first non-empty line is the header. We detect it by
        // checking that its first field is not a number.
        if (!headerSeen) {
            int maybeId = 0;
            if (!parseInt(fields.empty() ? std::string() : fields[0], maybeId)) {
                headerSeen = true;
                continue;  // this was the header row
            }
            headerSeen = true;
            // Not a header after all - fall through and parse it as data.
        }

        // We need at least id, name, type, latitude, longitude.
        if (fields.size() < 5) {
            markSkipped();
            continue;
        }

        Facility facility;
        if (!parseInt(fields[0], facility.id)) {
            markSkipped();
            continue;
        }

        facility.name = trim(fields[1]);
        // Types are normalised to lowercase here, and also on user input,
        // so that "Hospital" and "hospital" end up in the same group.
        facility.type = normalizeType(fields[2]);

        if (!parseNumber(fields[3], facility.lat) ||
            !parseNumber(fields[4], facility.lon)) {
            markSkipped();
            continue;
        }

        // Sanity check the coordinates: a latitude outside [-90, 90] or a
        // longitude outside [-180, 180] is a data error, not a location.
        if (facility.lat < -90.0 || facility.lat > 90.0 ||
            facility.lon < -180.0 || facility.lon > 180.0) {
            markSkipped();
            continue;
        }

        result.facilities.push_back(facility);
    }

    result.ok = true;
    return result;
}

}  // namespace csv_loader
