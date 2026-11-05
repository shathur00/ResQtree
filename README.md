# ResQTree

A small command-line nearest-emergency-facility finder built on a
hand-written KD-Tree (no external libraries beyond the C++ standard
library). Given a CSV of facilities, it can answer:

- nearest facility to a GPS point
- k nearest facilities
- all facilities within a radius (km)

Distances are great-circle (haversine) distances. Results can be filtered
by facility type (one type, a comma-separated list, or `all`).

## Build

```
make          # builds ./resqtree with zero warnings
make run      # build and run with default CSV
make clean    # remove build artifacts
```

## Run

```
./resqtree                 # uses data/facilities.csv
./resqtree path/to.csv     # use a custom CSV
```

## CSV format

```
id,name,type,latitude,longitude
1,City General Hospital,hospital,11.0168,76.9558
2,Red Cross Blood Bank,blood_bank,11.0183,76.9725
```

- first line is the header (skipped)
- blank lines and lines starting with `#` are skipped
- malformed rows (bad count, bad numbers, out-of-range coordinates,
  duplicate id) are skipped and counted
- types are normalized (lowercased + trimmed) on load

## Example session

```
$ ./resqtree
===== ResQTree =====
Loading data from: data/facilities.csv
Loaded 25 facilities across 7 types.
...
Your choice: 1
Available types:
  hospital : 6 facilities, tree height 3
...
Type(s) (e.g. hospital or hospital,blood_bank or all): hospital
Latitude: 11.01
Longitude: 76.95
rank  name                          type             id    distance (km)
1     Ampady Multi Speciality Hospital  hospital  2  0.32
```

## Menu

1. Find nearest facility  
2. Find k nearest facilities  
3. Find facilities within a radius  
4. Insert a facility  
5. Delete a facility (by id)  
6. Look up a facility by id  
7. List facility types and tree stats  
8. Rebalance all trees  
9. Exit
