import random
import sys
from bench import read_catalogue
from check import run


rng = random.Random(749863)
catalogue = read_catalogue()
cases = [[rng.choice(catalogue) for _ in range(n)] for n in
         (100, 120, 150, 200, 300, 500, 1000)]
scores = []
for area in (0, 13, 40, 80):
    for weight in (80, 160, 300):
        for shortlist in (2, 6, 12):
            results = []
            for pieces in cases:
                width = min(64, max(12, int(sum(map(len, pieces)) ** 0.5 * 0.54)))
                results.append(run(sys.argv[1], pieces,
                                   (width, 384, 0, 100, area, 0, 180, weight, shortlist)))
            score = sum(d for d, t in results) / len(results)
            scores.append((score, area, weight, shortlist))
            print(f"{score:.6f} area={area} cavity={weight} shortlist={shortlist} "
                  + " ".join(f"{d:.4f}" for d, t in results), flush=True)
print("best", sorted(scores, reverse=True)[:10])
