// kdtree.cpp
//
// Implementation of the 2D KD-Tree declared in kdtree.h.
// Everything here is written from scratch for the course project.

#include "kdtree.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "haversine.h"

// ---------------------------------------------------------------------------
// Building
// ---------------------------------------------------------------------------

// Build the subtree that owns the facilities in items[begin, end).
//
// The trick that makes this fast and balanced is std::nth_element:
//   - It rearranges the range so that the element at position `mid` is
//     the one that would be there if the range were fully sorted, and
//     everything before it is <= it and everything after it is >= it.
//   - It does that in O(n) time on average, instead of the O(n log n) a
//     full sort would cost.
//   - We then move the median element into the root of this subtree and
//     build the left child from the left half and the right child from
//     the right half. Because we always split in the middle, the tree
//     ends up perfectly balanced: height O(log n).
//
// The range is passed by reference because nth_element reorders it.
std::unique_ptr<KDTree::Node> KDTree::buildRange(std::vector<Facility>& items,
                                                  std::size_t begin, std::size_t end,
                                                  int depth) {
    // An empty range has no root at all.
    if (begin >= end) {
        return nullptr;
    }

    // The axis for this level: depth 0 -> latitude, depth 1 -> longitude,
    // depth 2 -> latitude again, and so on. This is the `depth % 2` rule.
    const int axis = depth % 2;

    // The median position. For a range of size n the median is at
    // begin + n/2, which leaves the two halves the same size (or the
    // left half one element bigger when n is even).
    const std::size_t mid = begin + (end - begin) / 2;

    // Put the median element at `mid` and partition the rest around it.
    std::nth_element(items.begin() + static_cast<std::ptrdiff_t>(begin),
                     items.begin() + static_cast<std::ptrdiff_t>(mid),
                     items.begin() + static_cast<std::ptrdiff_t>(end),
                     [axis](const Facility& a, const Facility& b) {
                         return coordinate(a, axis) < coordinate(b, axis);
                     });

    // Create the node for the median and take ownership of it.
    auto node = std::make_unique<Node>();
    node->facility = items[mid];

    // Recurse on the two halves. `depth + 1` makes the next level use
    // the other axis.
    node->left = buildRange(items, begin, mid, depth + 1);
    node->right = buildRange(items, mid + 1, end, depth + 1);

    return node;
}

void KDTree::build(std::vector<Facility> facilities) {
    // Start from a clean slate: a second call to build() replaces the
    // old tree completely rather than merging into it.
    root_.reset();
    count_ = 0;

    // An empty data set simply means an empty tree.
    if (facilities.empty()) {
        return;
    }

    root_ = buildRange(facilities, 0, facilities.size(), 0);
    count_ = facilities.size();
}

// ---------------------------------------------------------------------------
// Inserting
// ---------------------------------------------------------------------------

// Standard KD-Tree insert. We walk down from the root, comparing on the
// axis that belongs to the current depth. A facility with a smaller
// coordinate goes left, a facility with a larger-or-equal coordinate goes
// right. When we reach an empty slot we hang the new node there.
std::unique_ptr<KDTree::Node> KDTree::insertInto(std::unique_ptr<Node> node,
                                                   const Facility& f, int depth) {
    // Empty slot found - this is where the new facility belongs.
    if (!node) {
        auto fresh = std::make_unique<Node>();
        fresh->facility = f;
        ++count_;
        return fresh;
    }

    const int axis = depth % 2;

    if (coordinate(f, axis) < coordinate(node->facility, axis)) {
        node->left = insertInto(std::move(node->left), f, depth + 1);
    } else {
        node->right = insertInto(std::move(node->right), f, depth + 1);
    }

    return node;
}

void KDTree::insert(const Facility& f) {
    root_ = insertInto(std::move(root_), f, 0);
}

// ---------------------------------------------------------------------------
// Nearest neighbour
// ---------------------------------------------------------------------------

// Recursive nearest-neighbour search.
//
// The idea in one sentence: visit the child that is on the same side of
// the split as the query point FIRST (it is more likely to hold the
// answer), and only visit the other child if the splitting line is
// closer to the query point than the best answer we already have.
void KDTree::nearestIn(const Node* node, double lat, double lon, int depth,
                       NearestResult& best) const {
    // Step 1: an empty branch - nothing to look at.
    if (!node) {
        return;
    }

    // Step 2: how far is this node from the query point?
    const double distance = haversine::haversineKm(lat, lon,
                                                    node->facility.lat,
                                                    node->facility.lon);

    // Step 3: keep it if it beats the best answer so far.
    if (!best.has_value() || distance < best->second) {
        best = std::make_pair(node->facility, distance);
    }

    // Step 4: work out which side of the split we are on.
    const int axis = depth % 2;
    const double queryCoord = (axis == 0) ? lat : lon;
    const double splitCoord = coordinate(node->facility, axis);

    // "near" is the child on the same side of the split as the query
    // point, "far" is the child on the other side.
    const Node* near = nullptr;
    const Node* far = nullptr;
    if (queryCoord < splitCoord) {
        near = node->left.get();
        far = node->right.get();
    } else {
        near = node->right.get();
        far = node->left.get();
    }

    // Step 5: search the near child first. It holds the points that are
    // on the query's side of the split, so it is the more promising one.
    nearestIn(near, lat, lon, depth + 1, best);

    // Step 6: THE PRUNING STEP.
    //
    // The far child contains only points on the other side of the
    // splitting line. If we can prove that even the closest point of
    // that branch is further away than the best answer we already have,
    // then searching that branch cannot improve the result and we skip
    // it. The proof uses a *lower bound*: a number that is guaranteed to
    // be no larger than the real distance to anything in the branch.
    //
    //   lowerBound >= bestDistance  ==>  every point in the far child is
    //   at least as far away as the best answer we already have, so
    //   searching it cannot improve the result. We skip it.
    //
    //   lowerBound <  bestDistance  ==>  something in the far child might
    //   be closer, so we go and look.
    //
    // Axis 0 (latitude): the branch holds latitudes on one side of
    // splitCoord, so the distance to the parallel itself is the bound.
    //
    // Axis 1 (longitude): the branch holds a whole RANGE of longitudes,
    // and the Earth is round, so the distance to the splitting meridian
    // alone is NOT a safe bound (a branch near the antimeridian can hold
    // longitudes that are circularly much closer than the meridian).
    // lonFarBoundKm() therefore bounds the whole longitude range of the
    // branch - see haversine.h for the details.
    double lowerBound = 0.0;
    if (axis == 0) {
        lowerBound = haversine::latLineDistKm(lat, splitCoord);
    } else {
        // The far child is on the high side when the query point is on
        // the low side of the split, and vice versa.
        const bool farIsHigh = (queryCoord < splitCoord);
        lowerBound = haversine::lonFarBoundKm(lat, lon, splitCoord, farIsHigh);
    }

    if (lowerBound < best->second) {
        nearestIn(far, lat, lon, depth + 1, best);
    }
}

KDTree::NearestResult KDTree::nearest(double lat, double lon) const {
    // An empty tree has no answer at all.
    if (!root_) {
        return std::nullopt;
    }

    NearestResult best = std::nullopt;
    nearestIn(root_.get(), lat, lon, 0, best);
    return best;
}

// ---------------------------------------------------------------------------
// Bookkeeping
// ---------------------------------------------------------------------------

std::size_t KDTree::size() const {
    return count_;
}

bool KDTree::empty() const {
    return count_ == 0;
}

void KDTree::clear() {
    root_.reset();
    count_ = 0;
}
