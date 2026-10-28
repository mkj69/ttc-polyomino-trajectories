import importlib.util
import random
import time
from pathlib import Path

spec = importlib.util.spec_from_file_location("harness", "/logs/agent/multiagent-collab/anti-herding/work-1/harness.py")
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
words = iter(map(int, Path("/logs/agent/multiagent-collab/anti-herding/work-1/catalog.in").read_text().split()))
catalog = [tuple((next(words), next(words)) for _ in range(next(words))) for _ in range(next(words))]
classes = {shape: i for i, shape in enumerate(catalog)}
started = time.monotonic()
for width, height in ((5, 4), (4, 5), (6, 3), (3, 6), (4, 4), (5, 3)):
    rectangle = {(x, y) for x in range(width) for y in range(height)}
    edges = set()
    for i, shape in enumerate(catalog):
        if not 1 <= width * height - len(shape) <= 10:
            continue
        seen = set()
        for r in range(4):
            for f in range(2):
                oriented = h.normalize(h.transform(shape, r, f))
                if oriented in seen or (0, 0) not in oriented or not set(oriented) <= rectangle:
                    continue
                seen.add(oriented)
                complement = h.canonical(tuple(rectangle - set(oriented)))
                j = classes.get(complement)
                if j is not None:
                    edges.add(tuple(sorted((i, j))))
    means = []
    for n in (100, 1000, 10000):
        paired = []
        for seed in range(10):
            rng = random.Random(seed + 19233)
            counts = [0] * len(catalog)
            for _ in range(n):
                counts[rng.randrange(len(catalog))] += 1
            order = list(edges)
            rng.shuffle(order)
            pairs = 0
            for a, b in order:
                k = min(counts[a], counts[b]) if a != b else counts[a] // 2
                counts[a] -= k
                counts[b] -= k
                pairs += k
            paired.append(pairs)
        means.append(round(sum(paired) / len(paired), 1))
    print(f"rectangle={width}x{height} class_edges={len(edges)} mean_pairs_100_1000_10000={means}", flush=True)
print(f"seconds={time.monotonic() - started:.3f}")
