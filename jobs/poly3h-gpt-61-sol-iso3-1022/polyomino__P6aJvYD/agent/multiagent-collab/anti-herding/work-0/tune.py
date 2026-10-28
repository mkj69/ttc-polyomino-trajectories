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
for n in (120, 150, 200, 300, 500, 1000):
    cells = [rng.choice(catalogue) for _ in range(n)]
    lines = [str(n)]
    for shape in cells:
        lines.append(str(len(shape)))
        lines.extend(f"{x} {y}" for x, y in shape)
    inputs.append(("\n".join(lines)+"\n", sum(map(len, cells))))
configs = [(c, h, 10, 6, 0, window) for c, h, window in itertools.product((12, 24), (3, 9), (1, 4, 8, 16))]
start = time.monotonic()
ranked = []
for config in configs:
    ratios = []
    for text, area in inputs:
        p = subprocess.run([root + "solution"] + list(map(str, config)), input=text,
                           text=True, capture_output=True, check=True)
        w, h = map(int, p.stdout.split()[:2])
        ratios.append(area/(w*h))
    ranked.append((sum(ratios)/len(ratios), config, ratios))
    if len(ranked) % 24 == 0:
        print("tested", len(ranked), "best", sorted(ranked, reverse=True)[:3], flush=True)
for result in sorted(ranked, reverse=True)[:15]:
    print(result)
print("seconds", time.monotonic()-start)
