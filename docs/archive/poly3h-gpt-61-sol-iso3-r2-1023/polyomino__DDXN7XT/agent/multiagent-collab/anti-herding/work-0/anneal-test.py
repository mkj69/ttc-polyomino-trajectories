import random
import subprocess
import time

from check import all_free_shapes, transform
import sys


pool = all_free_shapes()
random.seed(832712)
executable = sys.argv[1] if len(sys.argv) > 1 else "./anneal"
for n in (100, 200, 500):
    for seed in range(5):
        pieces = [random.choice(pool) for _ in range(n)]
        cells = sum(map(len, pieces))
        lines = [str(n)]
        for p in pieces:
            lines.append(str(len(p)))
            lines.extend(f"{x} {y}" for x, y in p)
        text = "\n".join(lines) + "\n"
        for mode in (0, 1, 2, 3):
            start = time.monotonic()
            proc = subprocess.run([executable, "16", "0", "0", "0", "700", str(mode)],
                                  input=text, text=True, capture_output=True, check=True)
            values = list(map(int, proc.stdout.split()))
            w, h = values[:2]
            occupied = set()
            for i, shape in enumerate(pieces):
                x, y, r, f = values[2 + i * 4:6 + i * 4]
                for dx, dy in transform(shape, r, f):
                    cell = (x + dx, y + dy)
                    assert 0 <= cell[0] < w and 0 <= cell[1] < h
                    assert cell not in occupied
                    occupied.add(cell)
            print(n, seed, mode, proc.stderr.strip(), cells / (w * h),
                  time.monotonic() - start, flush=True)
