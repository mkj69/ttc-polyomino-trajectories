import os
import random
import subprocess
import time
from pathlib import Path

from harness import encode, shifted, validate

root = Path("/logs/agent/multiagent-collab/anti-herding/work-1")
words = iter(map(int, (root / "catalog.in").read_text().split()))
catalog = [tuple((next(words), next(words)) for _ in range(next(words)))
           for _ in range(next(words))]
totals = [0, 0]
times = [0, 0]
cases = 0
for n in (100, 333, 700, 1000):
    subtotals = [0, 0]
    for seed in range(4):
        rng = random.Random(17847 + n * 100 + seed)
        shapes = [shifted(rng.choice(catalog), rng) for _ in range(n)]
        data = encode(shapes)
        for width in (12, 16, 24):
            densities = []
            for i in range(2):
                args = [str(root / "unique_driver"), str(width),
                        "8" if n <= 700 else "4", "64" if n <= 400 else "0",
                        "7" if n <= 400 else "14"]
                start = time.monotonic()
                result = subprocess.run(args, input=data, capture_output=True,
                                        text=True, check=True, timeout=8,
                                        env=dict(os.environ, UNIQUE=str(i)))
                times[i] += time.monotonic() - start
                densities.append(validate(shapes, result.stdout)[0])
                totals[i] += densities[-1]
                subtotals[i] += densities[-1]
            cases += 1
            print(f"n={n} seed={seed} width={width} density={densities}", flush=True)
    print(f"n={n} means={[v/12 for v in subtotals]}", flush=True)
print(f"means={[v/cases for v in totals]} runtimes={times}", flush=True)
