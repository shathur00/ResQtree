# ResQTree - Viva Notes

## (a) How a KD-Tree is built and searched

A KD-Tree stores points in 2-D space (here, latitude and longitude).
Each node stores one point and splits space along one axis. The axis
alternates by depth: depth % 2 == 0 splits on latitude, depth % 2 == 1
splits on longitude.

Build: pick the median along the splitting axis using std::nth_element
and make it the root; recurse on the points left of the median (left
subtree) and right of it (right subtree). Median splits give a balanced
tree of height ~log n and build cost O(n log n) (each level does O(n)
work and there are log n levels).

Invariant: in a node splitting on axis a, every value in the left
subtree is <= the node's value on axis a, and every value in the right
subtree is >= it. Ties after build may land on either side; insert
sends equal values to the right.

Nearest search: start at the root and descend towards the side that
contains the query point first. Keep the best point found. On the way
back up, consider the other (far) side only if its lower bound (distance
from query to the splitting line/range) is less than the best distance so
far. This pruning is what makes the average cost O(log n).

k nearest: same traversal but with a max-heap of size k holding the k
closest found so far. Visit the far side if the heap is not full or the
lower bound <= the worst (largest) distance in the heap. Sort ascending
at the end.

Radius search: collect every node within radius. Visit the far side only
if its lower bound <= radius.

Deletion (standard KD-tree delete): when the node to delete has a right
child, replace it with the minimum on the current axis taken from the
right subtree and recursively delete that minimum; if there is no right
child, move the left subtree to the right first, then take the minimum.
When the coordinate on the current axis is EQUAL, search both children
(left then right) because ties may be on either side. A leaf is simply
removed.

## (b) Why pruning needs a lower bound; what if it is too large

The far subtree only needs to be examined if it COULD contain a better
answer. We compare a guaranteed-lower bound of the distance from the
query to every point in that subtree against the best distance so far.
If the lower bound is already >= best, no point there can beat it, so
the whole subtree is skipped.

If the bound is too small nothing breaks, we just do more work. If the
bound is too LARGE, a subtree that actually contains a closer point can
be wrongly pruned, returning a wrong (non-nearest) answer. That is why
the lower bounds must be proven valid, and the verification program
checks tree results against brute force for thousands of random queries.

## (c) Why longitude needs a wraparound-aware bound

Longitude lives in [-180, 180] but the Earth is round: -180 and 180 are
the same meridian. A split at 170 means its far (high) child covers
[170, 180], which wraps around to -180; a point at -179 is therefore
very close, not 1000+ km away.

A simple "distance to the splitting meridian" bound would measure ~1223
km and prune that branch, losing the true nearest. lonFarBoundKm instead
bounds by the nearest longitude in the WHOLE range of the branch using
R * asin(|cos(lat) * sin(lon - targetLon)|), minimised over the range
with minAbsSinOverRange. This correctly recognises that 175 and -179
are close. Target at -179, split at 170 handled.

## (d) Why one tree per facility type

Facilities of different types are queried independently ("hospitals
only", "hospital or blood_bank", "all"). Keeping one KD-Tree per
normalized type means each type is balanced on its own points, searches
never descend into nodes of unwanted types, and inserting/deleting a
facility touches only its own tree. An overall search runs on every
tree and merges results (best-of for nearest; concatenate + sort +
trim for k and radius).

## (e) Complexity

- build: O(n log n) time, O(n) space
- insert (no rebalance): O(h) expected O(log n), worst O(n)
- nearest / kNN: O(log n) average, O(n) worst case
- radius search: O(n) in the worst case (all points inside), but faster
  when the radius is small
- delete: O(n) worst (find-min can scan a subtree), average O(log n)
- toVector / clear / height / destructor: iterative (explicit stack),
  O(n) time and O(n) auxiliary space

## (f) Limitations

- Straight-line (great-circle) distance, not road distance.
- Insert has no rebalancing, so inserts can unbalance the tree; use
  Rebalance (toVector -> clear -> build) periodically.
- Worst-case behaviour degenerates to O(n) for degenerate (sorted
  input) trees; rebuild fixes it but automatic rebalancing is not done.
- Standard spherical approximation (Earth radius 6371 km), not WGS-84.

## Likely examiner questions

Q1. What is a KD-Tree and what does it store?
A. A binary space-partitioning tree; each node holds a point and splits
space along one alternating axis (here lat then lon).

Q2. How is the splitting axis chosen?
A. By depth: axis = depth % 2. Even depths split latitude, odd depths
split longitude.

Q3. Why std::nth_element for building?
A. It places the median in O(n) expected time, giving a balanced tree,
without fully sorting the vector.

Q4. What is the tree invariant?
A. Left subtree values <= node value on its axis; right subtree values >=
node value on its axis. Ties may be either side after build; insert sends
equal to the right.

Q5. Why visit both children on equal coordinate during delete/search?
A. Because ties may lie on either side of the split, so a point equal
on that axis can be in either child.

Q6. How does pruning work in nearest search?
A. The far side is searched only if its lower-bound distance to the
query point is less than the best distance found so far.

Q7. Why is latLineDistKm a valid lower bound?
A. It is the distance to the parallel at splitLat along the target's
meridian; every point beyond that parallel is at least that far.

Q8. Why is lonFarBoundKm valid and wraparound-aware?
A. It takes the minimum |sin| over the whole longitude range of the far
child, so it correctly treats longitudes near -180 and +180 as close.

Q9. Why one tree per type instead of one tree overall?
A. Type filtering stays efficient and each type stays balanced; overall
searches merge per-type results.

Q10. What is the worst-case time complexity of a search?
A. O(n) when the tree is degenerate or the radius is huge; average
O(log n) for a balanced tree.
