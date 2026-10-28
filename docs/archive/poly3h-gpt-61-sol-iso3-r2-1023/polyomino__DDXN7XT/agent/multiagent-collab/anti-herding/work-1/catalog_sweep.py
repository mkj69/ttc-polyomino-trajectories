import json
import subprocess
import time
from collections import defaultdict

from validate import decode, validate


variants = [
    ("profile256", None),
    ("full1", (1, 1, 1, 0, 0)),
    ("cap128d1", (1, 1, 1, 128, 0)),
    ("cap256d1", (1, 1, 1, 256, 0)),
    ("cap128d2", (2, 2, 2, 128, 0)),
    ("cap256d2", (2, 2, 2, 256, 0)),
    ("cap128area20", (2, 2, 2, 128, 20)),
    ("cap128area100", (2, 2, 2, 128, 100)),
]
rows = []
for n in [500, 1000, 2000, 5000, 10000]:
    for seed in range(261, 264):
        data = subprocess.run(
            ["./uniform", str(n), str(seed)], capture_output=True, text=True, check=True
        ).stdout
        pieces = decode(data)
        for name, config in variants:
            if config is None:
                command = [
                    "./beam-large-tuning", "32", "0", "0", "0",
                    "2", "2", "2", "6", "256", "10",
                ]
            else:
                states, depth, commit, cap, area = config
                command = [
                    "./catalog-frontier-tuning", "--catalog", "32", str(states),
                    str(depth), str(commit), "6", "1", "10", "500", str(cap), str(area),
                ]
            started = time.monotonic()
            result = subprocess.run(
                command, input=data, capture_output=True, text=True, check=True, timeout=30
            )
            seconds = time.monotonic() - started
            density = validate(pieces, result.stdout)
            rows.append({
                "n": n, "seed": seed, "name": name,
                "density": density, "seconds": seconds,
            })
        print(n, seed, [
            (r["name"], round(r["density"], 6), round(r["seconds"], 3))
            for r in rows[-len(variants):]
        ], flush=True)

with open("catalog-sweep.json", "w") as output:
    json.dump(rows, output)
groups = defaultdict(list)
for row in rows:
    groups[row["n"], row["name"]].append(row)
for key, values in groups.items():
    print(key, "density", sum(x["density"] for x in values) / len(values),
          "time", sum(x["seconds"] for x in values) / len(values), flush=True)
