import itertools
import random
import subprocess
import time

from check import all_free_shapes, ROOT


pool = all_free_shapes()
random.seed(72131)
inputs = {}
for n in (100, 300, 1000, 3000, 10000):
    shapes = [random.choice(pool) for _ in range(n)]
    lines = [str(n)]
    for shape in shapes:
        lines.append(str(len(shape)))
        lines += [f"{x} {y}" for x, y in shape]
    inputs[n] = (sum(map(len, shapes)), "\n".join(lines) + "\n")

for n, (cells, text) in inputs.items():
    results = []
    for width in (8, 10, 12, 16, 20, 24, 32, 48, 64, 96, 128, 192):
        start = time.monotonic()
        proc = subprocess.run(["./tune", str(width)], input=text,
                              text=True, capture_output=True, check=True, cwd=ROOT)
        w, h = map(int, proc.stdout.split()[:2])
        elapsed = time.monotonic() - start
        results.append((cells / (w * h), width, elapsed))
    print(n, "widths", sorted(results, reverse=True), flush=True)

text = inputs[1000][1]
for penalty, keep, candidates in itertools.product((100, 300, 600, 1000), (4, 12, 24), (24, 48, 96)):
    start = time.monotonic()
    proc = subprocess.run(["./tune", "24", str(candidates), str(penalty), str(keep)],
                          input=text, text=True, capture_output=True, check=True, cwd=ROOT)
    w, h = map(int, proc.stdout.split()[:2])
    print("parameter", penalty, keep, candidates, inputs[1000][0] / (w * h),
          time.monotonic() - start, flush=True)
