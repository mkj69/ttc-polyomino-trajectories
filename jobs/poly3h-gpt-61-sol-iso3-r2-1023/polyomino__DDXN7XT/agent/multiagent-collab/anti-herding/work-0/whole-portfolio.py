import json
import random
import statistics
import subprocess
import time
from check import all_free_shapes, transform

pool = all_free_shapes()
cases = []
for n in (100, 200):
    for seed in range(6):
        rng = random.Random(614732 + seed)
        shapes = [rng.choice(pool) for _ in range(n)]
        text = "\n".join([str(n)] + [
            str(len(s)) + "\n" + "\n".join(f"{x} {y}" for x, y in s)
            for s in shapes
        ]) + "\n"
        cases.append((n, seed, shapes, text))
configs = [
    (0, 40, 300, 32),
    (2, 40, 300, 32),
    (2, 40, 500, 32),
    (2, 20, 300, 32),
    (1, 40, 300, 32),
    (2, 40, 300, 64),
    (3, 40, 300, 32),
]
records = []
for cfg in configs:
    rows = []
    for n, seed, shapes, text in cases:
        started = time.monotonic()
        result = subprocess.run(
            ["./whole-portfolio"] + list(map(str, cfg)),
            input=text, text=True, capture_output=True, check=True, timeout=5,
        )
        elapsed = time.monotonic() - started
        tokens = list(map(int, result.stdout.split()))
        width, height = tokens[:2]
        assert len(tokens) == 2 + 4 * n
        occupied = set()
        for i, shape in enumerate(shapes):
            x, y, r, f = tokens[2 + 4 * i:6 + 4 * i]
            for xx, yy in transform(shape, r, f):
                cell = (x + xx, y + yy)
                assert 0 <= cell[0] < width and 0 <= cell[1] < height and cell not in occupied
                occupied.add(cell)
        rows.append(dict(n=n, seed=seed, density=sum(map(len, shapes)) / (width * height), time=elapsed))
    records.append(dict(config=cfg, rows=rows))
    with open("whole-portfolio.json", "w") as stream:
        json.dump(records, stream)
    print(cfg, [round(statistics.mean(row["density"] for row in rows if row["n"] == n), 6)
                for n in (100, 200)], flush=True)
