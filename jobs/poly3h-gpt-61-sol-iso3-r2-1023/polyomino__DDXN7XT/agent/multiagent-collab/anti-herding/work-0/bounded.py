import random
import subprocess
import time

from check import all_free_shapes, ROOT


pool = all_free_shapes()
random.seed(68127)
for n in (100, 300, 1000):
    wins, successes, tries = 0, 0, 0
    best_unbounded, best_bounded = [], []
    start = time.monotonic()
    for case in range(10):
        shapes = [random.choice(pool) for _ in range(n)]
        lines = [str(n)]
        for shape in shapes:
            lines.append(str(len(shape)))
            lines += [f"{x} {y}" for x, y in shape]
        text = "\n".join(lines) + "\n"
        cells = sum(map(len, shapes))
        width = max(10, min(48, int(cells ** 0.5 * 0.4)))
        best_area = 10**12
        for seed in range(10):
            proc = subprocess.run(["./tune", str(width), "48", "300", "12", "16",
                                   str(2**31 - 1), str(seed)],
                                  input=text, text=True, capture_output=True, check=True, cwd=ROOT)
            w, h = map(int, proc.stdout.split()[:2])
            best_area = min(best_area, w * h)
        unbounded_area = best_area
        for seed in range(10):
            target = (best_area - 1) // width
            proc = subprocess.run(["./tune", str(width), "48", "300", "12", "16",
                                   str(target), str(seed + 1000)],
                                  input=text, text=True, capture_output=True, check=True, cwd=ROOT)
            w, h = map(int, proc.stdout.split()[:2])
            tries += 1
            if w:
                best_area = min(best_area, w * h)
                successes += 1
        wins += best_area < unbounded_area
        best_unbounded.append(cells / unbounded_area)
        best_bounded.append(cells / best_area)
    print(n, "improved", wins, "successes", successes, tries,
          "baseline", sum(best_unbounded) / 10,
          "bounded", sum(best_bounded) / 10,
          "time", time.monotonic() - start, flush=True)
