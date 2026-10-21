// csv_loader.h
//
// Reading facilities from a CSV file.
//
// Expected format (first line is a header, which is skipped):
//
//   id,name,type,latitude,longitude
//   1,City General Hospital,hospital,11.0168,76.9558
//   2,Red Cross Blood Bank,blood_bank,11.0183,76.9725
//
// The loader is forgiving: it skips blank lines and comment lines that
// start with '#', and it tolerates extra spaces around every field.
#ifndef RESQTREE_CSV_LOADER_H
#define RESQTREE_CSV_LOADER_H

#include <string>
#include <vector>

#include "facility.h"

namespace csv_loader {

// The result of a load attempt.
struct LoadResult {
    bool ok = false;         // true when the file was read successfully
    std::vector<Facility> facilities;
    std::string error;       // a human readable message when ok == false
    std::size_t skipped = 0; // rows that were malformed and ignored
    std::size_t firstBadLine = 0; // line number of the first malformed row
                                  // (0 when nothing was skipped)
};

// Load every facility from the CSV file at `path`.
// On failure the returned result has ok == false and a non-empty error.
LoadResult load(const std::string& path);

// Split one CSV line into its fields, honouring double-quoted fields
// (a quoted field may contain commas). Exposed for testing.
std::vector<std::string> splitLine(const std::string& line);

}  // namespace csv_loader

#endif  // RESQTREE_CSV_LOADER_H
