import random
import subprocess
import time
from check import all_free_shapes


pool = all_free_shapes()
random.seed(17302798)
totals = [0.0] * 4
for n in (100, 200, 500):
    for seed in range(10):
        pieces = [random.choice(pool) for _ in range(n)]
        cells = sum(map(len, pieces))
        lines = [str(n)]
        for p in pieces:
            lines.append(str(len(p)))
            lines.extend(f"{x} {y}" for x, y in p)
        text = "\n".join(lines) + "\n"
        best = [0.0] * 4
        start = time.monotonic()
        for attempt in range(16):
            width = (10, 12, 16, 20)[attempt % 4] if n < 300 else (16, 20, 24, 32)[attempt % 4]
            for mode in range(4):
                noise = 30 if mode == 3 and attempt % 2 else 0
                weight = (0, 10, 25, (0, 10, 25, 10)[attempt % 4])[mode]
                proc = subprocess.run(
                    ["./grasp", str(width), "96", "16", "300", str(noise), str(weight),
                     "0", str(attempt)],
                    input=text, text=True, capture_output=True, check=True)
                w, h = map(int, proc.stdout.split()[:2])
                best[mode] = max(best[mode], cells / (w * h))
        for i, value in enumerate(best):
            totals[i] += value
        print(n, seed, [round(x, 6) for x in best],
              round(time.monotonic() - start, 2), flush=True)
print("means", [x / 30 for x in totals])
