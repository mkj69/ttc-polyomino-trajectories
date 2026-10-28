import os
import random
import subprocess
import sys
import time
from pathlib import Path

from harness import encode, shifted, validate

root = Path("/logs/agent/multiagent-collab/anti-herding/work-1")
words = iter(map(int, (root / "catalog.in").read_text().split()))
catalog = [tuple((next(words), next(words)) for _ in range(next(words)))
           for _ in range(next(words))]
modes = list(map(int, sys.argv[1:])) or [2, 4, 5, 6]
totals = [0] * len(modes)
times = [0] * len(modes)
cases = 0
for n in (100, 200, 333):
    subtotals = [0] * len(modes)
    for seed in range(5):
        rng = random.Random(17847 + n * 100 + seed)
        shapes = [shifted(rng.choice(catalog), rng) for _ in range(n)]
        data = encode(shapes)
        for width in (12, 16, 24):
            densities = []
            for i, mode in enumerate(modes):
                args = [str(root / "symmetry_driver"), str(width), "8", "64", "7"]
                start = time.monotonic()
                result = subprocess.run(args, input=data, capture_output=True,
                                        text=True, check=True, timeout=8,
                                        env=dict(os.environ, CANONICAL=str(mode)))
                times[i] += time.monotonic() - start
                densities.append(validate(shapes, result.stdout)[0])
                totals[i] += densities[-1]
                subtotals[i] += densities[-1]
            cases += 1
            print(f"n={n} seed={seed} width={width} density={densities}", flush=True)
    print(f"n={n} means={[v/15 for v in subtotals]}", flush=True)
print(f"modes={modes} means={[v/cases for v in totals]} runtimes={times}", flush=True)
