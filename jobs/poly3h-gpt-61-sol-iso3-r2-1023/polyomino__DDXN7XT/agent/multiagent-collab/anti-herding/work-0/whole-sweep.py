import random
import statistics
import subprocess
import time
from check import all_free_shapes, transform

pool = all_free_shapes()
cases = []
for n in (100, 200, 500):
    for seed in range(5):
        rng = random.Random(328427 + seed)
        shapes = [rng.choice(pool) for _ in range(n)]
        text = "\n".join([str(n)] + [
            str(len(s)) + "\n" + "\n".join(f"{x} {y}" for x, y in s)
            for s in shapes
        ]) + "\n"
        cases.append((n, seed, shapes, text))
configs = [
    (16, 16, 128, 20, 500),
    (16, 32, 128, 20, 500),
    (16, 64, 128, 20, 500),
    (16, 128, 128, 20, 500),
    (16, 32, 256, 20, 500),
    (16, 32, 512, 20, 500),
    (16, 32, 64, 20, 500),
    (16, 32, 128, 0, 500),
    (16, 32, 128, 10, 500),
    (16, 32, 128, 40, 500),
    (16, 32, 128, 80, 500),
    (16, 32, 128, 20, 300),
    (16, 32, 128, 20, 800),
    (16, 32, 128, 20, 1200),
    (12, 32, 128, 20, 500),
    (14, 32, 128, 20, 500),
    (18, 32, 128, 20, 500),
    (20, 32, 128, 20, 500),
    (24, 32, 128, 20, 500),
]
for cfg in configs:
    groups, seconds = [], []
    for n, seed, shapes, text in cases:
        w, states, count, hard, loss = cfg
        started = time.monotonic()
        result = subprocess.run(
            ["./whole-tuning", str(w), str(states), str(count), str(seed + 1), str(hard), str(loss)],
            input=text, text=True, capture_output=True, check=True, timeout=15,
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
        groups.append((n, sum(map(len, shapes)) / (width * height)))
        seconds.append((n, elapsed))
    print(cfg, [round(statistics.mean(x for nn, x in groups if nn == n), 6)
                for n in (100, 200, 500)],
          [round(sum(x for nn, x in seconds if nn == n), 3) for n in (100, 200, 500)],
          flush=True)
