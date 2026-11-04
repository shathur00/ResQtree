# Viva Notes

Plain-language notes. KD-Tree splits points by alternating axes; nearest uses pruning lower bounds.

Complexity: build O(n log n), insert O(log n) avg, delete O(log n), nearest O(log n) avg, kNN O(log n + k), radius O(sqrt(n)+m).

Limitations: straight-line not road, lat/lon box.