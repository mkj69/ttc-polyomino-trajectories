import random
import sys
from bench import read_catalogue
from check import run


rng = random.Random(67123)
catalogue = read_catalogue()
cases = [[rng.choice(catalogue) for _ in range(n)] for n in
         (100, 120, 150, 200, 300, 500, 1000, 1500)]
configs = []
if len(sys.argv) > 2 and sys.argv[2] == "width":
    for w in (10, 11, 12, 13, 14, 15, 16, 18, 20, 24, 28, 32, 40, 48, 64):
        results = [run(sys.argv[1], p, (w, 384)) for p in cases]
        print(w, " ".join(f"{d:.5f}" for d, t in results),
              f"mean={sum(d for d,t in results) / len(results):.6f}", flush=True)
else:
    for a in (-20, 0, 13, 30, 60):
        for h in (0, 12, 40, 100):
            for i in (0, 180, 400):
                configs.append((100, a, h, i))
    scores = []
    for config in configs:
        results = [run(sys.argv[1], p, (16 if len(p) < 300 else 32, 384, 0, *config))
                   for p in cases]
        score = sum(d for d, t in results) / len(results)
        scores.append((score, config))
    for score, config in sorted(scores, reverse=True)[:15]:
        print(f"score={score:.6f} config={config}", flush=True)
