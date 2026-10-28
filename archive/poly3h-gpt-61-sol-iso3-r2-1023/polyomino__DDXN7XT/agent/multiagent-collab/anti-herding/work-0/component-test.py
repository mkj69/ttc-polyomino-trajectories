import random
import subprocess
import time
import sys
from check import all_free_shapes, transform


random.seed(72918261)
pool = all_free_shapes()
totals = [0.0, 0.0]
times = [0.0, 0.0]
for n in (100, 200, 500, 1000):
    for seed in range(8):
        pieces = [random.choice(pool) for _ in range(n)]
        cells = sum(map(len, pieces))
        lines = [str(n)]
        for p in pieces:
            lines.append(str(len(p)))
            lines.extend(f"{x} {y}" for x, y in p)
        text = "\n".join(lines) + "\n"
        best = [0.0, 0.0]
        corner = sys.argv[1:] == ["corner"]
        executables = ("./corner", "./corner") if corner else ("./comp11", "./comp20")
        for mode, executable in enumerate(executables):
            for attempt in range(8):
                w = (10, 12, 16, 20)[attempt % 4] if n < 300 else (16, 20, 24, 32)[attempt % 4]
                start = time.monotonic()
                args = [executable, str(w), str(attempt)]
                if corner:
                    args += ["1", str(16 | (32 if mode else 0))]
                p = subprocess.run(args, input=text,
                                   text=True, capture_output=True, check=True)
                times[mode] += time.monotonic() - start
                values = list(map(int, p.stdout.split()))
                a, b = values[:2]
                occupied = set()
                for i, shape in enumerate(pieces):
                    x, y, r, f = values[2 + i * 4:6 + i * 4]
                    for dx, dy in transform(shape, r, f):
                        point = x + dx, y + dy
                        assert 0 <= point[0] < a and 0 <= point[1] < b
                        assert point not in occupied
                        occupied.add(point)
                best[mode] = max(best[mode], cells / (a * b))
        for i, value in enumerate(best):
            totals[i] += value
        print(n, seed, best, best[1] - best[0], flush=True)
print("means", [v / 32 for v in totals], "times", times)
