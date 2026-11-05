// kdtree.h
#ifndef RESQTREE_KDTREE_H
#define RESQTREE_KDTREE_H

#include <cstddef>
#include <memory>
#include <optional>
#include <queue>
#include <utility>
#include <vector>

#include "facility.h"
#include "haversine.h"

// One slot of the max-heap used by kNearest(). The heap is keyed by
// distance: the entry with the LARGEST distance sits on top so it can be
// replaced when a closer facility is found. std::priority_queue is a
// max-heap when the element type has operator< defined this way.
struct KDHeapItem {
    double dist = 0.0;
    Facility facility;
    bool operator<(const KDHeapItem& other) const { return dist < other.dist; }
};

// TREE INVARIANT (read this before touching the code):
//   For a node that splits on axis a:
//     - every value in the LEFT subtree is  <= the node's value on axis a
//     - every value in the RIGHT subtree is >= the node's value on axis a
//   Ties may therefore appear on either side after build() (nth_element
//   does not promise which side equal values land on). insert() sends equal
//   values to the RIGHT. Because of this, when deleting or searching for a
//   node whose coordinate on the current axis EQUALS the node's coordinate,
//   BOTH children have to be visited - see remove().
class KDTree {
public:
    using NearestResult = std::optional<std::pair<Facility, double>>;

    KDTree() = default;
    ~KDTree();

    void build(std::vector<Facility> facilities);
    void insert(const Facility& f);
    bool remove(const Facility& f);

    NearestResult nearest(double lat, double lon) const;
    std::vector<std::pair<Facility, double>> kNearest(double lat, double lon, std::size_t k) const;
    std::vector<std::pair<Facility, double>> radiusSearch(double lat, double lon, double radiusKm) const;

    std::size_t size() const;
    bool empty() const;
    void clear();
    int height() const;
    std::vector<Facility> toVector() const;
    void rebuild();

private:
    struct Node {
        Facility facility;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
    };

    std::unique_ptr<Node> root_;
    std::size_t count_ = 0;

    std::unique_ptr<Node> buildRange(std::vector<Facility>& items, std::size_t begin,
                                     std::size_t end, int depth);
    Node* findMin(Node* node, int axis, int depth) const;

    void nearestIn(const Node* node, double lat, double lon, int depth,
                   NearestResult& best) const;
    void kNearestIn(const Node* node, double lat, double lon, int depth, std::size_t k,
                    std::priority_queue<KDHeapItem>& heap) const;
    void radiusIn(const Node* node, double lat, double lon, double radiusKm, int depth,
                  std::vector<std::pair<Facility, double>>& out) const;

    bool removeNode(std::unique_ptr<Node>& node, const Facility& f, int depth);

    static double coordinate(const Facility& f, int axis) {
        return axis == 0 ? f.lat : f.lon;
    }
};

#endif