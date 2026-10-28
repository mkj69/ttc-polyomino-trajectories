import random
import subprocess
import time
from check import all_free_shapes


pool = all_free_shapes()
random.seed(21873549)
inputs = []
for n in (100, 200, 500):
    for seed in range(8):
        pieces = [random.choice(pool) for _ in range(n)]
        lines = [str(n)]
        for p in pieces:
            lines.append(str(len(p)))
            lines.extend(f"{x} {y}" for x, y in p)
        inputs.append((n, sum(map(len, pieces)), "\n".join(lines) + "\n"))

parameters = [(0, 0, 0)]
parameters += [(v, 0, 0) for v in (30, 70, 100, 150, 200, 300, 500)]
parameters += [(0, v, mode) for mode in (0, 1, 2) for v in (-30, -10, 10, 30)]
parameters += [(100, v, 0) for v in (-10, 10, 30)]
for noise, weight, mode in parameters:
    values = {n: [] for n in (100, 200, 500)}
    start = time.monotonic()
    for n, cells, text in inputs:
        width = 12 if n == 100 else 16 if n == 200 else 24
        proc = subprocess.run(
            ["./grasp", str(width), "96", "16", "300", str(noise), str(weight), str(mode)],
            input=text, text=True, capture_output=True, check=True)
        w, h = map(int, proc.stdout.split()[:2])
        values[n].append(cells / (w * h))
    print(noise, weight, mode,
          {n: round(sum(a) / len(a), 6) for n, a in values.items()},
          round(time.monotonic() - start, 3), flush=True)
