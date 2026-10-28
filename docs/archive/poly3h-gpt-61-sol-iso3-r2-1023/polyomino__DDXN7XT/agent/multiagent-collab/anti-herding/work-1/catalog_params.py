import json
import subprocess
import time
from collections import defaultdict
from validate import decode, validate

configs = [
    ("profile", None),
    ("c256d2", (256, 2, 2, 0)),
    ("c512d1", (512, 1, 1, 0)),
    ("c512d2", (512, 2, 2, 0)),
    ("c1024d1", (1024, 1, 1, 0)),
    ("c1024d2", (1024, 2, 2, 0)),
    ("c256random", (256, 2, 2, 1)),
    ("c512random", (512, 2, 2, 1)),
    ("c512shuffled", (512, 2, 2, 2)),
]
rows = []
for n in [2000, 5000, 10000]:
    for seed in range(271, 274):
        data = subprocess.run(["./uniform", str(n), str(seed)], check=True,
                              capture_output=True, text=True).stdout
        pieces = decode(data)
        for name, config in configs:
            if config is None:
                command = ["./beam-large-tuning", "32", "0", "0", "0",
                           "2", "2", "2", "6", "256", "10"]
            else:
                cap, states, depth, order = config
                command = ["./catalog-frontier-tuning", "--catalog", "32",
                           str(states), str(depth), str(depth), "6", "1", "10",
                           "500", str(cap), "0", str(order)]
            start = time.monotonic()
            p = subprocess.run(command, input=data, check=True, capture_output=True,
                               text=True, timeout=30)
            seconds = time.monotonic() - start
            rows.append(dict(n=n, seed=seed, name=name,
                             density=validate(pieces, p.stdout), seconds=seconds))
        print(n, seed, [(r["name"], round(r["density"], 6), round(r["seconds"], 3))
                       for r in rows[-len(configs):]], flush=True)
with open("catalog-params.json", "w") as output:
    json.dump(rows, output)
groups = defaultdict(list)
for r in rows:
    groups[r["n"], r["name"]].append(r)
for k, rr in groups.items():
    print(k, sum(x["density"] for x in rr) / len(rr),
          sum(x["seconds"] for x in rr) / len(rr), flush=True)
