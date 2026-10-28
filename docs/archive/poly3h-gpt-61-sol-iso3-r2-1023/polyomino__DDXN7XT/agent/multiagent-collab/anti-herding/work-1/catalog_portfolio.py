import json
import subprocess
import time
from collections import defaultdict
from validate import decode, validate

configs = [
    ("profile256", []),
    ("catalog512random", ["--catalog-portfolio", "2", "2", "2", "6", "512", "1"]),
    ("catalog256random", ["--catalog-portfolio", "2", "2", "2", "6", "256", "1"]),
]
rows = []
for n in [1500, 1501, 3000, 5000, 10000]:
    for seed in range(281, 284):
        data = subprocess.run(["./uniform", str(n), str(seed)], capture_output=True,
                              text=True, check=True).stdout
        pieces = decode(data)
        for name, args in configs:
            t = time.monotonic()
            p = subprocess.run(["./catalog-frontier-tuning", *args], input=data,
                               capture_output=True, text=True, check=True, timeout=5)
            seconds = time.monotonic() - t
            rows.append(dict(n=n, seed=seed, name=name, seconds=seconds,
                             density=validate(pieces, p.stdout)))
        print(n, seed, [(r["name"], round(r["density"], 6), round(r["seconds"], 3))
                       for r in rows[-len(configs):]], flush=True)
with open("catalog-portfolio.json", "w") as output:
    json.dump(rows, output)
groups = defaultdict(list)
for r in rows:
    groups[r["n"], r["name"]].append(r)
for k, rr in groups.items():
    print(k, sum(x["density"] for x in rr) / len(rr),
          sum(x["seconds"] for x in rr) / len(rr), flush=True)
