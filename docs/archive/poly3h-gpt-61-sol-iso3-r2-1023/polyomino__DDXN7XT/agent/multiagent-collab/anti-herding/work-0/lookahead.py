import random
import subprocess
import time

from check import all_free_shapes, ROOT


pool = all_free_shapes()
random.seed(89231)
for n in (100, 300, 1000, 3000):
    results = {variant: [] for variant in (0, 16, 18)}
    for case in range(5):
        shapes = [random.choice(pool) for _ in range(n)]
        lines = [str(n)]
        for shape in shapes:
            lines.append(str(len(shape)))
            lines += [f"{x} {y}" for x, y in shape]
        text = "\n".join(lines) + "\n"
        cells = sum(map(len, shapes))
        width = max(10, min(48, int(cells ** 0.5 * 0.4)))
        for variant in results:
            start = time.monotonic()
            proc = subprocess.run(["./tune", str(width), "48", "300", "12", str(variant)],
                                  input=text, text=True, capture_output=True, check=True, cwd=ROOT)
            w, h = map(int, proc.stdout.split()[:2])
            results[variant].append((cells / (w * h), time.monotonic() - start))
    print(n, results, flush=True)
