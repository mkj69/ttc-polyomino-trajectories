import itertools
import random
import subprocess
import time

root = "/logs/agent/multiagent-collab/anti-herding/work-0/"
data = iter(map(int, open(root + "catalogue.txt").read().split()))
catalogue = []
for _ in range(next(data)):
    catalogue.append([(next(data), next(data)) for _ in range(next(data))])
rng = random.Random(70073)
inputs = []
for n in (100, 100, 150, 150, 300, 300):
    cells = [rng.choice(catalogue) for _ in range(n)]
    lines = [str(n)]
    for shape in cells:
        lines.append(str(len(shape)))
        lines.extend(f"{x} {y}" for x, y in shape)
    area = sum(map(len, cells))
    inputs.append(("\n".join(lines)+"\n", area))
configs = list(itertools.product((16, 32), (4,), (20, 60), (0, 80, 160, 300)))
start = time.monotonic()
ranked = []
for beam, branches, small, cavity in configs:
    ratios = []
    for text, area in inputs:
        w = int(area**0.5 * 0.6)
        p = subprocess.run([root + "solution", str(w), str(beam), str(branches),
                            "30", "100", str(small), str(cavity), "0", "1"],
                           input=text, text=True, capture_output=True, check=True)
        width, height = map(int, p.stdout.split()[:2])
        ratios.append(area/(width*height))
    ranked.append((sum(ratios)/len(ratios), (beam, branches, small, cavity), ratios))
    if len(ranked) % 8 == 0:
        print("tested", len(ranked), "best", sorted(ranked, reverse=True)[:3], flush=True)
for result in sorted(ranked, reverse=True)[:15]:
    print(result)
print("seconds", time.monotonic()-start)
