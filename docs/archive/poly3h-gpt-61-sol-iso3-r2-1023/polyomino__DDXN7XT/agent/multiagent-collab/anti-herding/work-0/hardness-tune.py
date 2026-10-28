import random
import subprocess
import sys
from check import all_free_shapes


pool = all_free_shapes()
holdout = sys.argv[1:] == ["holdout"]
random.seed(581927 if not holdout else 739271625)
cases = []
for n in (100, 200, 500):
    for seed in range(5 if not holdout else 10):
        pieces = [random.choice(pool) for _ in range(n)]
        lines = [str(n)]
        for p in pieces:
            lines.append(str(len(p)))
            lines.extend(f"{x} {y}" for x, y in p)
        cases.append((n, sum(map(len, pieces)), "\n".join(lines) + "\n"))

configs = [(w, 0, 0, 0, 0) for w in (0, 5, 10, 15, 20, 30, 50, 80)]
configs += [(10, w, 0, 0, 0) for w in (-20, -10, 10, 20, 40)]
configs += [(10, 0, w, 0, 0) for w in (-20, -10, 10, 20, 40)]
configs += [(10, 0, 0, w, 0) for w in (-20, -10, 10, 20, 40)]
configs += [(10, 0, 0, 0, w) for w in (-40, -20, -10, 10, 20, 40)]
if holdout:
    configs = [(10, 0, 0, v, 0) for v in (0, -10, 20, 40)]
    configs += [(5, 0, 0, v, 0) for v in (20, 40)]
elif len(sys.argv) > 1:
    configs = configs[int(sys.argv[1])::3]
for weight, perimeter, symmetry, width, regret in configs:
    totals = {n: 0.0 for n in (100, 200, 500)}
    for n, cells, text in cases:
        best = 0.0
        for attempt in range(8 if not holdout else 12):
            w = (10, 12, 16, 20)[attempt % 4] if n < 300 else (16, 20, 24, 32)[attempt % 4]
            proc = subprocess.run(
                ["./grasp", str(w), "96", "16", "300", "0", str(weight), "0", str(attempt),
                 str(perimeter), str(symmetry), str(width), str(regret)],
                input=text, text=True, capture_output=True, check=True)
            a, b = map(int, proc.stdout.split()[:2])
            best = max(best, cells / (a * b))
        totals[n] += best
    print((weight, perimeter, symmetry, width, regret),
          {n: round(v / (5 if not holdout else 10), 6) for n, v in totals.items()}, flush=True)
