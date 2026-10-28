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
modes = [(0, 0), (2, 0), (2, 1), (2, 2)]
totals = [0.0] * len(modes)
times = [0.0] * len(modes)
gains = [0] * len(modes)
cases = 0
for n in (100, 200, 333):
    for seed in range(3):
        rng = random.Random(17847 + n * 100 + seed)
        shapes = [shifted(rng.choice(catalog), rng) for _ in range(n)]
        data = encode(shapes)
        for width in (12, 16, 24):
            densities = []
            for i, (raw, norm) in enumerate(modes):
                args = [str(root / "cell_driver"), str(width), "8", "16", "7", "1"]
                start = time.monotonic()
                result = subprocess.run(args, input=data, capture_output=True,
                                        text=True, check=True, timeout=8,
                                        env=dict(os.environ, PROGRESS_RAW=str(raw),
                                                 PROGRESS_NORMALIZE=str(norm)))
                times[i] += time.monotonic() - start
                densities.append(validate(shapes, result.stdout)[0])
                totals[i] += densities[-1]
                gains[i] += densities[-1] > densities[0]
            cases += 1
            print(f"n={n} seed={seed} width={width} density={densities}", flush=True)
print(f"modes={modes} gains={gains}/{cases} "
      f"means={[v/cases for v in totals]} runtimes={times}", flush=True)
