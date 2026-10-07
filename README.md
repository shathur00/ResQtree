# ResQTree

**Fast emergency facility search using KD-Trees**

A C++17 command-line tool that finds the nearest hospital, blood bank, or ambulance base to a given location, without checking every facility one by one. Built as a project for Advanced Data Structures.

---

## Problem

In an emergency, finding the nearest help quickly matters. A simple approach compares the caller's location against every facility, which takes O(n) time per query. This becomes slow for large datasets or many simultaneous requests.

## Solution

ResQTree stores facility coordinates (latitude, longitude) in a **KD-Tree**. During a search, whole regions of the tree that cannot contain a closer facility are skipped, giving about **O(log n)** average query time.

## Features

- Load facilities from a CSV file
- Nearest facility search
- k-nearest facilities search
- Radius (range) search
- Insert and delete facilities
- Optional filter by facility type
- Benchmark mode: KD-Tree vs brute-force search, with a correctness check

## Data Structures Used

| Structure | Purpose |
|---|---|
| KD-Tree (k = 2) | Nearest-neighbour and range queries |
| Max-heap (`std::priority_queue`) | Keeps the k best results during k-NN search |
| Hash map (`std::unordered_map`) | O(1) lookup of facility details by ID |

## Complexity

| Operation | Average | Worst |
|---|---|---|
| Build | O(n log n) | O(n log n) |
| Insert | O(log n) | O(n) |
| Nearest neighbour | O(log n) | O(n) |
| Range query | O(√n + m) | O(n) |
| Space | O(n) | O(n) |

*m = number of results returned.*

## Project Structure

```
ResQTree/
├── src/
│   ├── main.cpp          # CLI menu
│   ├── kdtree.h / .cpp   # KD-Tree implementation
│   ├── bruteforce.h      # Baseline linear search
│   ├── facility.h        # Facility data structure
│   ├── haversine.h       # Distance calculation
│   └── benchmark.cpp     # Benchmark code
├── data/
│   └── facilities.csv    # Sample dataset
├── report/
│   └── abstract.tex
├── Makefile
└── README.md
```

## Build and Run

**Requirements:** a C++17 compiler (g++ or clang++)

```bash
# Build
g++ -std=c++17 -O2 src/*.cpp -o resqtree

# Run
./resqtree data/facilities.csv
```

## Usage

```
===== ResQTree =====
1. Nearest facility
2. k nearest facilities
3. Facilities within radius
4. Insert facility
5. Delete facility
6. Run benchmark
7. Exit
```

## Input Format

CSV with a header row:

```
id,name,type,latitude,longitude
1,City General Hospital,hospital,11.0168,76.9558
2,Red Cross Blood Bank,blood_bank,11.0183,76.9725
```

Data can come from OpenStreetMap (via Overpass Turbo), government open data portals such as data.gov.in, or randomly generated points for benchmarking.

## Benchmark

KD-Tree and brute-force nearest-neighbour search are compared on increasing dataset sizes. Both must return identical results, which also verifies the KD-Tree is correct.

| Dataset size | Brute force (avg ms/query) | KD-Tree (avg ms/query) | Speedup |
|---|---|---|---|
| 1,000 | TBD | TBD | TBD |
| 10,000 | TBD | TBD | TBD |
| 100,000 | TBD | TBD | TBD |
| 1,000,000 | TBD | TBD | TBD |

*Compiled with `-O2`. Results to be filled in after running benchmark mode.*

## Notes and Limitations

- Distances use the **Haversine formula** for real-world accuracy. KD-Tree pruning assumes flat geometry, which is a good approximation over city or regional scales.
- Results show straight-line distance, not road travel time.
- Repeated inserts can unbalance the tree, so the tree can be rebuilt periodically.

## Possible Future Work

- Road-distance ranking using Dijkstra on a shortlist from the KD-Tree
- Graphical map visualisation (e.g. raylib)
- Filtering by live capacity (e.g. available ICU beds)
