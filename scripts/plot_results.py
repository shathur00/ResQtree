import csv, os
try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
except Exception as e:
    print("matplotlib not available:", e)
    raise SystemExit(0)
# placeholder
print("done")