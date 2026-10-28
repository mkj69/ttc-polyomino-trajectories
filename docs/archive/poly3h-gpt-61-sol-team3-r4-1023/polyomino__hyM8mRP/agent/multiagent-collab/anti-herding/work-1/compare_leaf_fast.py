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
times = [0.0, 0.0]
totals = [0.0, 0.0]
mismatches = 0
cases = 0
for n in (100, 333, 700, 1000):
    for seed in range(3):
        rng = random.Random(17847 + n * 100 + seed)
        shapes = [shifted(rng.choice(catalog), rng) for _ in range(n)]
        data = encode(shapes)
        for width in (12, 16, 24):
            outputs, densities = [], []
            for i, binary in enumerate(("leaf_base_driver", "leaf_fast_driver")):
                args = [str(root / binary), str(width), "8",
                        "64" if n <= 400 else "32", "7" if n <= 400 else "14"]
                start = time.monotonic()
                result = subprocess.run(args, input=data, capture_output=True,
                                        text=True, check=True, timeout=8,
                                        env=dict(os.environ, UNIQUE="0"))
                times[i] += time.monotonic() - start
                densities.append(validate(shapes, result.stdout)[0])
                totals[i] += densities[-1]
                outputs.append(result.stdout)
            mismatches += outputs[0] != outputs[1]
            cases += 1
            print(f"n={n} seed={seed} width={width} same={outputs[0] == outputs[1]} "
                  f"densities={densities}", flush=True)
print(f"mismatches={mismatches}/{cases} runtimes={times} "
      f"means={[v/cases for v in totals]}", flush=True)
