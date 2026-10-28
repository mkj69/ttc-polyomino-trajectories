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
budgets = list(map(int, sys.argv[1:])) or [0, 200]
totals = [0.0] * len(budgets)
times = [0.0] * len(budgets)
cases = 0
for n in (100, 150, 200, 333):
    for seed in range(4):
        rng = random.Random(17847 + n * 100 + seed)
        shapes = [shifted(rng.choice(catalog), rng) for _ in range(n)]
        data = encode(shapes)
        densities = []
        diagnostics = []
        for i, budget in enumerate(budgets):
            start = time.monotonic()
            result = subprocess.run([str(root / "wide")], input=data, capture_output=True,
                                    text=True, check=True, timeout=8,
                                    env=dict(os.environ, WIDE=str(budget), WIDE_DEBUG="1"))
            times[i] += time.monotonic() - start
            densities.append(validate(shapes, result.stdout)[0])
            totals[i] += densities[-1]
            diagnostics.append(result.stderr.strip())
        cases += 1
        print(f"n={n} seed={seed} density={densities} "
              f"diagnostics={diagnostics}", flush=True)
print(f"budgets={budgets} means={[v/cases for v in totals]} runtimes={times}", flush=True)
