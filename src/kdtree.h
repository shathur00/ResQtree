// kdtree.h
//
// A hand-written 2D KD-Tree storing Facility objects.
//
// WHAT IS A KD-TREE?
//   A KD-Tree is a binary search tree whose "key" is not a single value
//   but a point in k dimensions. At every node the tree picks ONE axis to
//   compare on. Everything with a smaller coordinate on that axis goes to
//   the left child, everything with a larger-or-equal coordinate goes to
//   the right child. The next level down switches to the other axis, and
//   so on.
//
// WHAT DOES "DEPTH" MEAN HERE?
//   "Depth" is simply "which round of splitting are we in".
//     depth 0 = the root, which splits on latitude  (axis 0)
//     depth 1 = its children, which split on longitude (axis 1)
//     depth 2 = the next round down, which splits on latitude again
//   So the axis is chosen with `depth % 2`:
//     axis 0 = latitude,  axis 1 = longitude.
//   Because the axis alternates on every level, the tree behaves like a
//   binary search tree along BOTH coordinates at once, which is what makes
//   fast nearest-neighbour lookups possible.
//
// The implementation is deliberately written from scratch (no third-party
// KD-Tree library) for an Advanced Data Structures course project.
#ifndef RESQTREE_KDTREE_H
#define RESQTREE_KDTREE_H

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "facility.h"

class KDTree {
public:
    // The result of a nearest-neighbour query: the facility itself and
    // the distance to it in kilometres.
    using NearestResult = std::optional<std::pair<Facility, double>>;

    KDTree() = default;

    // Build (or rebuild) the tree from a list of facilities.
    // The tree is balanced: each node is placed at the median of the
    // points it is responsible for, so the height is O(log n).
    // Rebuilding an existing tree discards the old contents.
    void build(std::vector<Facility> facilities);

    // Insert a single facility into an existing tree.
    // NOTE: repeated inserts can make the tree unbalanced, so `build`
    // is the better choice when you have the whole data set at once.
    void insert(const Facility& f);

    // Find the facility closest to (lat, lon).
    // Returns std::nullopt when the tree is empty.
    NearestResult nearest(double lat, double lon) const;

    // Number of facilities currently stored.
    std::size_t size() const;

    // True when the tree holds no facilities at all.
    bool empty() const;

    // Drop every facility.
    void clear();

private:
    // One node of the tree. It holds the facility plus the links to its
    // two children. unique_ptr gives us automatic memory management:
    // when a Node is destroyed its children are destroyed too, so there
    // is never a leak and never a dangling pointer.
    struct Node {
        Facility facility;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
    };

    std::unique_ptr<Node> root_;
    std::size_t count_ = 0;

    // Build the subtree from items[begin, end) at the given depth and
    // return its root. The vector is reordered in place while building -
    // that is how std::nth_element finds the medians for us.
    std::unique_ptr<Node> buildRange(std::vector<Facility>& items,
                                     std::size_t begin, std::size_t end,
                                     int depth);

    // Insert into the subtree rooted at `node`, choosing the axis from
    // the depth. Returns the (possibly new) root of that subtree.
    std::unique_ptr<Node> insertInto(std::unique_ptr<Node> node, const Facility& f, int depth);

    // Recursive nearest-neighbour search. `best` is the best answer found
    // so far (and its distance); `bestDistance` is a shorthand for
    // best->second.
    void nearestIn(const Node* node, double lat, double lon, int depth,
                   NearestResult& best) const;

    // The coordinate of a facility on the given axis:
    //   axis 0 -> latitude,  axis 1 -> longitude.
    static double coordinate(const Facility& f, int axis) {
        return axis == 0 ? f.lat : f.lon;
    }
};

#endif  // RESQTREE_KDTREE_H
