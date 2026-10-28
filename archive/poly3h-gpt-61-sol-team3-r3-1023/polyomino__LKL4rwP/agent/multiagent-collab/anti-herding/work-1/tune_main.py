from pathlib import Path
from statistics import mean

from check import ROOT, generate, library, run


shapes = library()
binary = ROOT / "main-eval"
for n, width, seeds in [(100, 15, range(10)), (1000, 26, range(5)), (10000, 60, range(2))]:
    for lookahead in (0, 2, 4, 8, 12):
        densities = []
        times = []
        for seed in seeds:
            pieces = generate(shapes, n, seed + 134, "uniform")
            density, elapsed, w, h = run(binary, [width, 320, 37, 37, 0, 712367, lookahead, 10], pieces)
            densities.append(density)
            times.append(elapsed)
        print(f"n={n} width={width} lookahead={lookahead} mean_density={mean(densities):.6f} "
              f"mean_time={mean(times):.4f} max_time={max(times):.4f}", flush=True)
