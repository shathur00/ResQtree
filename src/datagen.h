#ifndef RESQTREE_DATAGEN_H
#define RESQTREE_DATAGEN_H

#include <string>
#include <vector>

#include "facility.h"

namespace datagen {
std::vector<Facility> generate(std::size_t n, const std::string& distribution, unsigned seed,
                               const std::string& type = "synthetic");
}
#endif