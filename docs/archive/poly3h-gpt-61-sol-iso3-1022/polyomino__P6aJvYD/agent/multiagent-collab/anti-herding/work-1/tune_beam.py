import random
import sys
import time
from bench import read_catalogue
from check import run


rng = random.Random(918235)
catalogue = read_catalogue()
cases = [[rng.choice(catalogue) for _ in range(n)] for n in
         (100, 100, 150, 150, 200, 300)]
configs = []
configs.extend([(32, 4, 30, 100, 60, 160, mode) for mode in (0, 1, 2)])
for _ in range(97):
    configs.append((rng.choice((16, 32, 48)), rng.choice((3, 4, 6)),
                    rng.choice((10, 30, 60, 100)), rng.choice((30, 100, 300)),
                    rng.choice((0, 20, 60, 100, 160)),
                    rng.choice((0, 80, 160, 300)), rng.choice((1, 2))))
results = []
start = time.monotonic()
for config in configs:
    scores = []
    for pieces in cases:
        width = max(12, int(sum(map(len, pieces)) ** .5 * .6))
        scores.append(run(sys.argv[1], pieces, (width, *config)))
    mean = sum(d for d, t in scores) / len(scores)
    duration = sum(t for d, t in scores)
    results.append((mean, duration, config))
    print(f"{mean:.6f} {duration:.3f}s {config} " +
          " ".join(f"{d:.4f}" for d, t in scores), flush=True)
print("BEST", sorted(results, reverse=True)[:15])
print("COST", time.monotonic() - start, "seconds", len(configs) * len(cases), "cases")
