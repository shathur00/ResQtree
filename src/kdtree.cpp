#include "kdtree.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stack>
#include <utility>
#include <vector>

#include "haversine.h"

KDTree::~KDTree() {
    if (!root_) return;
    std::vector<Node*> toDelete;
    toDelete.push_back(root_.release());
    while (!toDelete.empty()) {
        Node* n = toDelete.back();
        toDelete.pop_back();
        if (n->left) toDelete.push_back(n->left.release());
        if (n->right) toDelete.push_back(n->right.release());
        delete n;
    }
}

std::unique_ptr<KDTree::Node> KDTree::buildRange(std::vector<Facility>& items,
                                                  std::size_t begin, std::size_t end,
                                                  int depth) {
    if (begin >= end) return nullptr;
    const int axis = depth % 2;
    const std::size_t mid = begin + (end - begin) / 2;
    std::nth_element(items.begin() + static_cast<std::ptrdiff_t>(begin),
                     items.begin() + static_cast<std::ptrdiff_t>(mid),
                     items.begin() + static_cast<std::ptrdiff_t>(end),
                     [axis](const Facility& a, const Facility& b) {
                         return coordinate(a, axis) < coordinate(b, axis);
                     });
    auto node = std::make_unique<Node>();
    node->facility = items[mid];
    node->left = buildRange(items, begin, mid, depth + 1);
    node->right = buildRange(items, mid + 1, end, depth + 1);
    return node;
}

void KDTree::build(std::vector<Facility> facilities) {
    root_.reset();
    count_ = 0;
    if (facilities.empty()) return;
    root_ = buildRange(facilities, 0, facilities.size(), 0);
    count_ = facilities.size();
}

void KDTree::insert(const Facility& f) {
    if (!root_) {
        root_ = std::make_unique<Node>();
        root_->facility = f;
        ++count_;
        return;
    }
    Node* cur = root_.get();
    int depth = 0;
    while (true) {
        const int axis = depth % 2;
        if (coordinate(f, axis) < coordinate(cur->facility, axis)) {
            if (!cur->left) {
                cur->left = std::make_unique<Node>();
                cur->left->facility = f;
                ++count_;
                return;
            }
            cur = cur->left.get();
        } else {
            if (!cur->right) {
                cur->right = std::make_unique<Node>();
                cur->right->facility = f;
                ++count_;
                return;
            }
            cur = cur->right.get();
        }
        ++depth;
    }
}

KDTree::Node* KDTree::findMin(Node* node, int axis, int depth) const {
    if (!node) return nullptr;
    Node* best = node;
    const int nodeAxis = depth % 2;
    if (nodeAxis == axis) {
        Node* leftMin = findMin(node->left.get(), axis, depth + 1);
        if (leftMin && coordinate(leftMin->facility, axis) < coordinate(best->facility, axis)) {
            best = leftMin;
        }
        // right subtree holds >= values, cannot contain smaller
    } else {
        Node* leftMin = findMin(node->left.get(), axis, depth + 1);
        Node* rightMin = findMin(node->right.get(), axis, depth + 1);
        if (leftMin && coordinate(leftMin->facility, axis) < coordinate(best->facility, axis)) best = leftMin;
        if (rightMin && coordinate(rightMin->facility, axis) < coordinate(best->facility, axis)) best = rightMin;
    }
    return best;
}

bool KDTree::removeNode(std::unique_ptr<Node>& node, const Facility& f, int depth) {
    if (!node) return false;
    const int axis = depth % 2;

    if (node->facility.id == f.id) {
        if (node->right) {
            Node* mn = findMin(node->right.get(), axis, depth + 1);
            node->facility = mn->facility;
            return removeNode(node->right, mn->facility, depth + 1);
        } else if (node->left) {
            Node* mn = findMin(node->left.get(), axis, depth + 1);
            auto oldLeft = std::move(node->left);
            node->right = std::move(oldLeft);
            node->facility = mn->facility;
            return removeNode(node->right, mn->facility, depth + 1);
        } else {
            node.reset();
            return true;
        }
    }

    const double targetCoord = coordinate(f, axis);
    const double nodeCoord = coordinate(node->facility, axis);

    bool found = false;
    if (targetCoord < nodeCoord) {
        found = removeNode(node->left, f, depth + 1);
    } else if (targetCoord > nodeCoord) {
        found = removeNode(node->right, f, depth + 1);
    } else {
        // Equal coordinate on this axis: ties may be on EITHER side, so
        // the searched node can be in either child. Try left first, and
        // if that failed try right.
        if (removeNode(node->left, f, depth + 1)) found = true;
        else found = removeNode(node->right, f, depth + 1);
    }
    return found;
}

bool KDTree::remove(const Facility& f) {
    bool ok = removeNode(root_, f, 0);
    if (ok) --count_;
    return ok;
}

void KDTree::nearestIn(const Node* node, double lat, double lon, int depth,
                       NearestResult& best) const {
    if (!node) return;

    const double distance = haversine::haversineKm(lat, lon, node->facility.lat, node->facility.lon);
    if (!best.has_value() || distance < best->second) {
        best = std::make_pair(node->facility, distance);
    }

    const int axis = depth % 2;
    const double queryCoord = (axis == 0) ? lat : lon;
    const double splitCoord = coordinate(node->facility, axis);

    const Node* near = nullptr;
    const Node* far = nullptr;
    if (queryCoord < splitCoord) {
        near = node->left.get();
        far = node->right.get();
    } else {
        near = node->right.get();
        far = node->left.get();
    }

    nearestIn(near, lat, lon, depth + 1, best);

    if (!far) return;
    const bool farIsHigh = (queryCoord < splitCoord);
    double lowerBound = 0.0;
    if (axis == 0) lowerBound = haversine::latLineDistKm(lat, splitCoord);
    else lowerBound = haversine::lonFarBoundKm(lat, lon, splitCoord, farIsHigh);

    if (!best.has_value() || lowerBound < best->second) {
        nearestIn(far, lat, lon, depth + 1, best);
    }
}

KDTree::NearestResult KDTree::nearest(double lat, double lon) const {
    if (!root_) return std::nullopt;
    NearestResult best = std::nullopt;
    nearestIn(root_.get(), lat, lon, 0, best);
    return best;
}

void KDTree::kNearestIn(const Node* node, double lat, double lon, int depth, std::size_t k,
                        std::priority_queue<KDHeapItem>& heap) const {
    if (!node) return;

    const double distance = haversine::haversineKm(lat, lon, node->facility.lat, node->facility.lon);
    if (heap.size() < k) {
        heap.push({distance, node->facility});
    } else if (!heap.empty() && distance < heap.top().dist) {
        heap.pop();
        heap.push({distance, node->facility});
    }

    const int axis = depth % 2;
    const double queryCoord = (axis == 0) ? lat : lon;
    const double splitCoord = coordinate(node->facility, axis);

    const Node* near = nullptr;
    const Node* far = nullptr;
    if (queryCoord < splitCoord) {
        near = node->left.get();
        far = node->right.get();
    } else {
        near = node->right.get();
        far = node->left.get();
    }

    kNearestIn(near, lat, lon, depth + 1, k, heap);

    if (!far) return;
    const bool farIsHigh = (queryCoord < splitCoord);
    double lowerBound = (axis == 0) ? haversine::latLineDistKm(lat, splitCoord)
                                   : haversine::lonFarBoundKm(lat, lon, splitCoord, farIsHigh);

    if (heap.size() < k || lowerBound <= heap.top().dist) {
        kNearestIn(far, lat, lon, depth + 1, k, heap);
    }
}

std::vector<std::pair<Facility, double>> KDTree::kNearest(double lat, double lon, std::size_t k) const {
    if (k == 0 || !root_) return {};
    std::priority_queue<KDHeapItem> heap;
    kNearestIn(root_.get(), lat, lon, 0, k, heap);
    // sort ascending
    std::vector<std::pair<Facility, double>> res;
    res.reserve(heap.size());
    while (!heap.empty()) {
        auto top = heap.top(); heap.pop();
        res.push_back({top.facility, top.dist});
    }
    std::sort(res.begin(), res.end(), [](const auto& a, const auto& b){ return a.second < b.second; });
    return res;
}

void KDTree::radiusIn(const Node* node, double lat, double lon, double radiusKm, int depth,
                      std::vector<std::pair<Facility, double>>& out) const {
    if (!node) return;

    const double distance = haversine::haversineKm(lat, lon, node->facility.lat, node->facility.lon);
    if (distance <= radiusKm) {
        out.push_back({node->facility, distance});
    }

    const int axis = depth % 2;
    const double queryCoord = (axis == 0) ? lat : lon;
    const double splitCoord = coordinate(node->facility, axis);

    const Node* near = nullptr;
    const Node* far = nullptr;
    if (queryCoord < splitCoord) {
        near = node->left.get();
        far = node->right.get();
    } else {
        near = node->right.get();
        far = node->left.get();
    }

    radiusIn(near, lat, lon, radiusKm, depth + 1, out);

    if (!far) return;
    const bool farIsHigh = (queryCoord < splitCoord);
    double lowerBound = (axis == 0) ? haversine::latLineDistKm(lat, splitCoord)
                                   : haversine::lonFarBoundKm(lat, lon, splitCoord, farIsHigh);
    if (lowerBound <= radiusKm) {
        radiusIn(far, lat, lon, radiusKm, depth + 1, out);
    }
}

std::vector<std::pair<Facility, double>> KDTree::radiusSearch(double lat, double lon, double radiusKm) const {
    if (radiusKm < 0 || !root_) return {};
    std::vector<std::pair<Facility, double>> out;
    radiusIn(root_.get(), lat, lon, radiusKm, 0, out);
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.second < b.second; });
    return out;
}

std::size_t KDTree::size() const { return count_; }
bool KDTree::empty() const { return count_ == 0; }

void KDTree::clear() {
    // iterative delete
    if (root_) {
        std::vector<Node*> toDelete;
        toDelete.push_back(root_.release());
        while (!toDelete.empty()) {
            Node* n = toDelete.back(); toDelete.pop_back();
            if (n->left) toDelete.push_back(n->left.release());
            if (n->right) toDelete.push_back(n->right.release());
            delete n;
        }
    }
    count_ = 0;
}

int KDTree::height() const {
    if (!root_) return 0;
    int h = 0;
    std::stack<std::pair<Node*, int>> st;
    st.push({root_.get(), 1});
    while (!st.empty()) {
        auto [n, d] = st.top(); st.pop();
        h = std::max(h, d);
        if (n->left) st.push({n->left.get(), d + 1});
        if (n->right) st.push({n->right.get(), d + 1});
    }
    return h;
}

std::vector<Facility> KDTree::toVector() const {
    std::vector<Facility> out;
    if (!root_) return out;
    std::stack<Node*> st;
    st.push(root_.get());
    while (!st.empty()) {
        Node* n = st.top(); st.pop();
        out.push_back(n->facility);
        if (n->left) st.push(n->left.get());
        if (n->right) st.push(n->right.get());
    }
    return out;
}

void KDTree::rebuild() {
    auto v = toVector();
    clear();
    build(std::move(v));
}