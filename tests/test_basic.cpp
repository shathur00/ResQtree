// test_basic.cpp
//
// Self-contained sanity tests for ResQTree.
//
// These are the "does the thing actually work" checks: distances that are
// known by hand, the pruning bounds that the KD-Tree relies on, and a
// brute-force cross-check that the KD-Tree returns the same nearest
// facility as a plain linear scan.
//
// The file has its own main(), so it is a separate program from the
// resqtree application. Run it with:  make test
//
// The project headers are found via the -Isrc flag that the Makefile
// passes when it builds this test.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "csv_loader.h"
#include "facility.h"
#include "facility_index.h"
#include "haversine.h"
#include "kdtree.h"

namespace {

// ---------------------------------------------------------------------------
// A tiny test framework: count the checks and the failures.
// ---------------------------------------------------------------------------
int gChecks = 0;
int gFailures = 0;

void check(bool condition, const std::string& what) {
    ++gChecks;
    if (condition) {
        std::cout << "  [ok]   " << what << "\n";
    } else {
        ++gFailures;
        std::cout << "  [FAIL] " << what << "\n";
    }
}

void checkClose(double actual, double expected, double tolerance,
                const std::string& what) {
    ++gChecks;
    if (std::fabs(actual - expected) <= tolerance) {
        std::cout << "  [ok]   " << what << "\n";
    } else {
        ++gFailures;
        std::cout << "  [FAIL] " << what << " (expected " << expected
                  << ", got " << actual << ")\n";
    }
}

void section(const std::string& title) {
    std::cout << "\n" << title << "\n";
}

// A helper that makes a facility without repeating five fields every time.
Facility makeFacility(int id, const std::string& name, const std::string& type,
                      double lat, double lon) {
    Facility f;
    f.id = id;
    f.name = name;
    f.type = type;
    f.lat = lat;
    f.lon = lon;
    return f;
}

// The plain, obvious way to find the nearest facility: look at all of
// them. Used only inside the tests to check that the KD-Tree agrees.
std::pair<Facility, double> bruteForceNearest(const std::vector<Facility>& all,
                                               double lat, double lon) {
    std::pair<Facility, double> best = {Facility(), std::numeric_limits<double>::infinity()};
    for (const Facility& f : all) {
        const double d = haversine::haversineKm(lat, lon, f.lat, f.lon);
        if (d < best.second) {
            best = {f, d};
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testHaversine() {
    section("Haversine distance");

    // The distance from a point to itself is zero.
    checkClose(haversine::haversineKm(11.0, 76.0, 11.0, 76.0), 0.0, 1e-9,
               "distance to the same point is 0");

    // One degree of latitude is about 111.19 km anywhere on the globe.
    // (1 degree * R * pi/180 = 6371 * 0.0174533 = 111.19 km)
    checkClose(haversine::haversineKm(0.0, 0.0, 1.0, 0.0), 111.19, 0.05,
               "1 degree of latitude is about 111.19 km");

    // The equator: one degree of longitude is also about 111.19 km.
    checkClose(haversine::haversineKm(0.0, 0.0, 0.0, 1.0), 111.19, 0.05,
               "1 degree of longitude at the equator is about 111.19 km");

    // Towards the poles the same longitude step covers less ground:
    // 111.19 * cos(60 degrees) = about 55.6 km at 60 degrees north.
    checkClose(haversine::haversineKm(60.0, 0.0, 60.0, 1.0), 55.6, 0.1,
               "1 degree of longitude at 60N is about 55.6 km");

    // A well known reference distance: the straight-line (great-circle)
    // distance from Kochi to Chennai is about 560 km. The road distance
    // is longer (roughly 680 km) - the Haversine formula gives the
    // straight line, not the route.
    const double southIndia = haversine::haversineKm(9.9312, 76.2673, 13.0827, 80.2707);
    check(southIndia > 540.0 && southIndia < 580.0,
          "Kochi to Chennai is in the 540-580 km range");
    checkClose(southIndia, 559.5, 1.0, "Kochi to Chennai is about 560 km");
}

void testLowerBounds() {
    section("Pruning lower bounds");

    // The lower bound to a parallel must never exceed the real distance
    // to any point on that parallel. Pick a point directly "north" of the
    // target and one far to the side, and check both.
    const double targetLat = 10.0;
    const double targetLon = 77.0;
    const double splitLat = 11.0;

    const double bound = haversine::latLineDistKm(targetLat, splitLat);
    checkClose(bound, 111.19, 0.05, "latitude bound matches 1 degree of latitude");

    // The point on the split parallel that lies on the target's own
    // meridian is the closest one, and the bound must equal that distance.
    const double direct = haversine::haversineKm(targetLat, targetLon, splitLat, targetLon);
    check(bound <= direct + 1e-9, "latitude bound never exceeds the real distance");

    // A point on the split parallel but at a different longitude is
    // further away, so the bound is still valid for it.
    const double sideways = haversine::haversineKm(targetLat, targetLon, splitLat, 78.0);
    check(bound <= sideways + 1e-9, "latitude bound valid for an off-meridian point");

    // Longitude bound: same story on the other axis.
    const double lonBound = haversine::lonLineDistKm(targetLat, targetLon, 78.0);
    const double lonDirect = haversine::haversineKm(targetLat, targetLon, targetLat, 78.0);
    check(lonBound <= lonDirect + 1e-9, "longitude bound never exceeds the real distance");
    // The bound is the *perpendicular* distance to the meridian, which is
    // slightly shorter than the great-circle distance to the point where
    // the meridian crosses the target's parallel (the two are joined by
    // a right angle at the equator). The difference here is about 0.1 m,
    // so a tolerance of 1 metre is plenty.
    checkClose(lonBound, lonDirect, 0.001, "longitude bound matches the meridian distance");

    // The far-child bound must cover the WHOLE longitude range of the
    // branch, not just the splitting meridian. This is the case that a
    // meridian-only bound gets wrong: the target is one degree past the
    // antimeridian, the tree splits at 170 degrees east, and the far
    // child holds longitudes 170..180 - which includes 175, only six
    // degrees away from the target. A bound based on the meridian alone
    // would say 1223 km here and prune away a much closer facility.
    const double farBound = haversine::lonFarBoundKm(0.0, -179.0, 170.0, true);
    const double realFarDistance = haversine::haversineKm(0.0, -179.0, 0.0, 175.0);
    check(farBound <= realFarDistance + 1e-9,
          "far-child bound never exceeds the real distance to a point in the branch");
    check(farBound < 700.0,
          "far-child bound stays small enough to keep the nearby branch");

    // And the bound for the other side of the split must be valid too.
    const double lowBound = haversine::lonFarBoundKm(0.0, -179.0, 170.0, false);
    const double realLowDistance = haversine::haversineKm(0.0, -179.0, 0.0, -175.0);
    check(lowBound <= realLowDistance + 1e-9,
          "far-child bound is valid on the low side of the split as well");

    // More than 90 degrees apart: the function must be conservative and
    // return 0 so that the KD-Tree does not prune that branch.
    checkClose(haversine::lonLineDistKm(targetLat, targetLon, 170.0), 0.0, 1e-12,
               "longitude bound is 0 when more than 90 degrees apart");

    // At the pole every meridian meets, so the bound is 0.
    checkClose(haversine::lonLineDistKm(90.0, 0.0, 120.0), 0.0, 1e-9,
               "longitude bound is 0 at the pole");
}

void testKDTreeBasics() {
    section("KD-Tree basics");

    KDTree empty;
    check(empty.empty(), "a new tree is empty");
    check(empty.size() == 0, "a new tree has size 0");
    check(!empty.nearest(10.0, 77.0).has_value(), "an empty tree has no nearest facility");

    // One facility.
    KDTree single;
    single.build({makeFacility(1, "Only", "hospital", 10.0, 77.0)});
    check(single.size() == 1, "a tree with one facility has size 1");
    check(!single.empty(), "a tree with one facility is not empty");

    const auto result = single.nearest(10.0, 77.0);
    check(result.has_value(), "a single-facility tree finds a result");
    if (result.has_value()) {
        check(result->first.id == 1, "the single facility is the one returned");
        checkClose(result->second, 0.0, 1e-9, "the distance to itself is 0");
    }

    // Building again replaces the contents, it does not add to them.
    single.build({makeFacility(2, "Other", "hospital", 12.0, 79.0)});
    check(single.size() == 1, "rebuilding replaces the tree contents");

    // Insert.
    KDTree inserted;
    inserted.insert(makeFacility(1, "A", "hospital", 10.0, 77.0));
    inserted.insert(makeFacility(2, "B", "hospital", 12.0, 79.0));
    inserted.insert(makeFacility(3, "C", "hospital", 11.0, 78.0));
    check(inserted.size() == 3, "insert adds three facilities");

    const auto insertedResult = inserted.nearest(11.0, 78.0);
    check(insertedResult.has_value() && insertedResult->first.id == 3,
          "insert puts the exact match first");
}

void testKDTreeAgainstBruteForce() {
    section("KD-Tree vs brute force");

    // A deterministic pseudo random generator, so the test always runs
    // on exactly the same data.
    std::vector<Facility> all;
    unsigned int seed = 12345;
    for (int i = 0; i < 500; ++i) {
        seed = seed * 1103515245u + 12345u;
        const double lat = -80.0 + 160.0 * (static_cast<double>(seed % 100000) / 100000.0);
        seed = seed * 1103515245u + 12345u;
        const double lon = -180.0 + 360.0 * (static_cast<double>(seed % 100000) / 100000.0);
        all.push_back(makeFacility(i + 1, "F" + std::to_string(i + 1), "hospital", lat, lon));
    }

    KDTree tree;
    tree.build(all);
    check(tree.size() == all.size(), "the tree holds every facility after build");

    // Compare against the brute force answer for a set of query points.
    int mismatches = 0;
    double worstDifference = 0.0;
    for (int q = 0; q < 200; ++q) {
        seed = seed * 1103515245u + 12345u;
        const double qLat = -80.0 + 160.0 * (static_cast<double>(seed % 100000) / 100000.0);
        seed = seed * 1103515245u + 12345u;
        const double qLon = -180.0 + 360.0 * (static_cast<double>(seed % 100000) / 100000.0);

        const auto got = tree.nearest(qLat, qLon);
        const auto expected = bruteForceNearest(all, qLat, qLon);

        if (!got.has_value()) {
            ++mismatches;
            continue;
        }
        // Compare the distances, and the ids when they are close enough
        // to be the same facility.
        worstDifference = std::max(worstDifference, std::fabs(got->second - expected.second));
        if (got->second > expected.second + 1e-6) {
            ++mismatches;
        }
    }
    check(mismatches == 0, "the KD-Tree never returns a worse distance than brute force");
    check(worstDifference < 1e-6, "the KD-Tree distance matches brute force exactly");

    // A query on one of the data points must find that point.
    const auto exact = tree.nearest(all[42].lat, all[42].lon);
    check(exact.has_value() && exact->first.id == all[42].id,
          "a query on a stored facility returns that facility");
    check(exact.has_value() && exact->second < 1e-6, "the distance to itself is 0");
}

void testFacilityIndex() {
    section("Facility index");

    FacilityIndex index;
    check(index.empty(), "a new index is empty");

    const std::vector<Facility> facilities = {
        makeFacility(1, "Hospital A", "Hospital", 10.0, 77.0),
        makeFacility(2, "Hospital B", "hospital", 12.0, 79.0),
        makeFacility(3, "Bus Stop", "bus_stop", 10.5, 77.5),
        makeFacility(4, "Shop", "medical_shop", 9.0, 76.0),
    };
    index.build(facilities);

    check(index.size() == 4, "the index holds every facility");
    check(index.countOfType("hospital") == 2, "\"Hospital\" and \"hospital\" are one type");
    check(index.countOfType("bus_stop") == 1, "the bus stop is in its own type");
    check(index.countOfType("medical_shop") == 1, "the shop is in its own type");
    check(index.countOfType("unknown") == 0, "an unknown type has a count of 0");

    const std::vector<std::string> types = index.types();
    check(types.size() == 3, "there are three distinct types");
    check(types.size() == 3 && types[0] == "bus_stop" && types[1] == "hospital" &&
              types[2] == "medical_shop",
          "the types are listed in alphabetical order");

    // The nearest hospital to the bus stop is Hospital A, not Hospital B.
    const auto hospital = index.nearestInType("Hospital", 10.5, 77.5);
    check(hospital.has_value() && hospital->first.id == 1,
          "the nearest hospital is found in its own tree");
    check(hospital.has_value() && hospital->first.type == "hospital",
          "the result carries the normalised type");
    check(!index.nearestInType("unknown", 10.0, 77.0).has_value(),
          "an unknown type returns no result");

    // The overall search picks the closest facility of any type.
    const auto overall = index.nearestOverall(9.9, 77.1);
    check(overall.has_value() && overall->first.id == 1,
          "the overall search finds the closest facility of any type");

    // Adding a facility to a brand new type creates its tree.
    index.add(makeFacility(5, "Pharmacy", "pharmacy", 9.5, 76.5));
    check(index.countOfType("pharmacy") == 1, "adding a new type creates a new tree");
    check(index.size() == 5, "the index grew by one");
}

void testCsvLoader() {
    section("CSV loader");

    const std::vector<std::string> simple = csv_loader::splitLine("1,Hospital,hospital,10.0,77.0");
    check(simple.size() == 5, "a plain line splits into five fields");
    check(simple.size() == 5 && simple[0] == "1" && simple[4] == "77.0",
          "the fields keep their order");

    const std::vector<std::string> quoted = csv_loader::splitLine("2,\"Red Cross, Central\",blood_bank,10.5,77.5");
    check(quoted.size() == 5, "a quoted field with a comma still gives five fields");
    check(quoted.size() == 5 && quoted[1] == "Red Cross, Central",
          "the quoted field keeps its comma");

    // splitLine() splits the line but does not trim it; the loader trims
    // each field afterwards. So the raw fields keep their spaces here.
    const std::vector<std::string> spaced = csv_loader::splitLine(" 3 , Clinic , clinic , 9.0 , 76.0 ");
    check(spaced.size() == 5, "a line with padded fields still has five fields");
    check(spaced.size() == 5 && trim(spaced[1]) == "Clinic",
          "the padded field trims down to the name");

    // A file that does not exist must fail cleanly instead of crashing.
    const csv_loader::LoadResult missing = csv_loader::load("/no/such/file.csv");
    check(!missing.ok, "loading a missing file reports a failure");
    check(!missing.error.empty(), "the failure carries a message");
}

void testTypeNormalisation() {
    section("Type normalisation");

    check(normalizeType("  Hospital ") == "hospital", "spaces and case are normalised");
    check(normalizeType("BLOOD_BANK") == "blood_bank", "upper case becomes lower case");
    check(normalizeType("Bus Stop") == "bus stop", "inner spaces are kept as they are");
}

}  // namespace

int main() {
    std::cout << "===== ResQTree basic tests =====\n";

    testHaversine();
    testLowerBounds();
    testKDTreeBasics();
    testKDTreeAgainstBruteForce();
    testFacilityIndex();
    testCsvLoader();
    testTypeNormalisation();

    std::cout << "\n----------------------------------------\n";
    std::cout << gChecks << " checks, " << gFailures << " failure(s)\n";

    if (gFailures == 0) {
        std::cout << "ALL TESTS PASSED\n";
        return EXIT_SUCCESS;
    }

    std::cout << "TESTS FAILED\n";
    return EXIT_FAILURE;
}
