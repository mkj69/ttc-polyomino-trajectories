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
improvements = 0
cases = 0
for n in (100, 200, 333):
    for seed in range(5):
        rng = random.Random(17847 + n * 100 + seed)
        shapes = [shifted(rng.choice(catalog), rng) for _ in range(n)]
        data = encode(shapes)
        for width in (12, 16, 24):
            densities = []
            for i in range(2):
                args = [str(root / "bounded_driver"), str(width), "8", "64", "7"]
                if i:
                    args += ["1"]
                start = time.monotonic()
                result = subprocess.run(args, input=data, capture_output=True,
                                        text=True, check=True, timeout=12)
                times[i] += time.monotonic() - start
                densities.append(validate(shapes, result.stdout)[0])
                totals[i] += densities[-1]
            improvements += densities[1] > densities[0]
            cases += 1
            print(f"n={n} seed={seed} width={width} density={densities} "
                  f"{result.stderr.strip()}", flush=True)
print(f"improvements={improvements}/{cases} means={[v/cases for v in totals]} "
      f"runtimes={times}", flush=True)
