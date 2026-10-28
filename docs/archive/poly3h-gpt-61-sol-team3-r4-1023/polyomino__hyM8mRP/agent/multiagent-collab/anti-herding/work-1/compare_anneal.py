import random
import subprocess
import time
from pathlib import Path

from harness import encode, shifted, validate


root = Path("/logs/agent/multiagent-collab/anti-herding/work-1")
words = iter(map(int, (root / "catalog.in").read_text().split()))
catalog = []
for _ in range(next(words)):
    catalog.append(tuple((next(words), next(words)) for _ in range(next(words))))
rng = random.Random(10262026)
for n in (100, 333, 1000):
    densities = [[], []]
    runtimes = [[], []]
    for trial in range(5):
        shapes = [shifted(rng.choice(catalog), rng) for _ in range(n)]
        total = sum(map(len, shapes))
        width = max(10, int(2.6 * total ** 0.25))
        data = encode(shapes)
        for i, binary in enumerate(("frontier_exact", "anneal")):
            started = time.monotonic()
            args = [str(root / binary), str(width), "200", "50"]
            args += ["30", "2", "3", "1"]
            if i:
                args += ["1"]
            output = subprocess.run(args,
                                    input=data, capture_output=True, text=True, check=True).stdout
            runtimes[i].append(time.monotonic() - started)
            densities[i].append(validate(shapes, output)[0])
    print(f"n={n} base={sum(densities[0])/5:.7f} "
          f"anneal={sum(densities[1])/5:.7f} "
          f"times={sum(runtimes[0]):.3f},{sum(runtimes[1]):.3f}", flush=True)
