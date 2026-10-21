# ResQTree

**Fast emergency facility search using KD-Trees**

A C++17 command-line tool that finds the nearest hospital, blood bank, or
ambulance base to a given location, without checking every facility one by
one. Built as a project for Advanced Data Structures.

---

## Problem

In an emergency, finding the nearest help quickly matters. A simple approach
compares the caller's location against every facility, which takes O(n) time
per query. This becomes slow for large datasets or many simultaneous requests.

## Solution

ResQTree stores facility coordinates (latitude, longitude) in a **KD-Tree**.
During a search, whole regions of the tree that cannot contain a closer
facility are skipped, giving about **O(log n)** average query time.

## Features (version 1)

- Load facilities from a CSV file
- Nearest facility search, restricted to one type or across all types
- One separate KD-Tree per facility type
- Correct great-circle (Haversine) distances
- A self-contained test suite, including a brute-force cross-check

**Not in version 1** (planned for later versions): k-nearest search, radius
search, delete, benchmarking against brute force as a program feature, and a
GUI. The code is structured so these can be added without touching the
KD-Tree implementation.

## Data Structures Used

| Structure | Purpose |
|---|---|
| KD-Tree (k = 2) | Nearest-neighbour queries, one per facility type |
| Hash map (`std::unordered_map`) | Maps a type name to its KD-Tree, O(1) lookup |

## Complexity

| Operation | Average | Worst |
|---|---|---|
| Build | O(n log n) | O(n log n) |
| Insert | O(log n) | O(n) |
| Nearest neighbour | O(log n) | O(n) |
| Space | O(n) | O(n) |

## Project Structure

```
ResQTree/
├── src/
│   ├── main.cpp              # CLI menu and program flow
│   ├── facility.h            # Facility struct, trimming and normalisation
│   ├── haversine.h           # Distance functions and pruning lower bounds
│   ├── kdtree.h / kdtree.cpp # KD-Tree implementation
│   ├── facility_index.h/.cpp # One KD-Tree per facility type
│   └── csv_loader.h/.cpp     # CSV reading
├── tests/
│   └── test_basic.cpp        # Self-contained sanity tests
├── data/
│   └── facilities.csv        # Sample dataset
├── Makefile
└── README.md
```

## Build and Run

**Requirements:** a C++17 compiler (g++ or clang++)

```bash
# Build
make

# Build and run
make run

# Build and run the tests
make test

# Remove all build output
make clean
```

Running `./resqtree` with no arguments works out of the box: the program
looks for `data/facilities.csv` first and then `../data/facilities.csv`. An
optional first argument overrides the path:

```bash
./resqtree data/facilities.csv
```

## Usage

```
===== ResQTree =====
1. Nearest facility search
2. Show data set summary
0. Exit
```

The search asks for a latitude and a longitude, then optionally a facility
type. Leave the type empty (or choose "nearest of any type") to search every
type at once.

## Input Format

CSV with a header row:

```
id,name,type,latitude,longitude
1,City General Hospital,hospital,11.0168,76.9558
2,Red Cross Blood Bank,blood_bank,11.0183,76.9725
```

Types are normalised to lowercase on load and on user input, so `Hospital`,
`HOSPITAL` and ` hospital ` all end up in the same group. Malformed rows are
skipped and counted rather than aborting the load.

Data can come from OpenStreetMap (via Overpass Turbo), government open data
portals such as data.gov.in, or randomly generated points for benchmarking.

## How the KD-Tree Works

The tree is a binary search tree whose key is a *point* rather than a single
value. At each node one axis is chosen: axis 0 is latitude, axis 1 is
longitude. The axis alternates with the depth, so the root splits on
latitude, its children split on longitude, the next level splits on latitude
again, and so on. Points with a smaller coordinate go left, larger-or-equal
go right.

`build()` uses `std::nth_element` to place the median of each range at the
root of that subtree, so the tree comes out perfectly balanced (height
O(log n)) and the whole build is O(n log n).

### Pruning

A nearest-neighbour search visits the child on the same side of the split
as the query point first. The other child is only searched if it can still
contain something closer. This uses a **lower bound** on the distance: a
number that is guaranteed to be no larger than the real distance to
anything in that branch. If the bound is already worse than the best
answer found so far, the branch is skipped.

The bounds live in `haversine.h`:

- `latLineDistKm` - distance to the parallel through the splitting point.
- `lonFarBoundKm` - distance to the nearest longitude in the *whole* range
  occupied by the branch.

The second one deserves a note. The obvious bound, "distance to the
splitting meridian", is **not** safe on a round Earth. If the target is at
longitude -179 and the tree splits at longitude 170, the far branch holds
longitudes 170..180 - and the point at longitude 175 in that branch is only
6 degrees of longitude from the target, i.e. much closer than the 1223 km
the meridian-based bound would suggest. Pruning on the meridian alone
discards a genuinely nearer facility. `lonFarBoundKm` therefore bounds the
whole longitude range of the branch, and the test suite includes a
brute-force cross-check over the full latitude and longitude range to catch
this class of bug.

## Notes and Limitations

- Distances use the **Haversine formula** for real-world accuracy. KD-Tree
  pruning assumes flat geometry, which is a good approximation over city or
  regional scales.
- Results show straight-line distance, not road travel time.
- Repeated inserts can unbalance the tree, so the tree can be rebuilt
  periodically.

## Possible Future Work

- k-nearest facilities, radius search, and delete
- Benchmark mode: KD-Tree vs brute-force, with a correctness check
- Road-distance ranking using Dijkstra on a shortlist from the KD-Tree
- Graphical map visualisation (e.g. raylib)
- Filtering by live capacity (e.g. available ICU beds)
