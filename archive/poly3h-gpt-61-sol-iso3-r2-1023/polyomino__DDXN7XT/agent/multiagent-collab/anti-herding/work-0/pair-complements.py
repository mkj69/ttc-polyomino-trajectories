import collections
import random
import time
from check import all_free_shapes, orientations

pool = all_free_shapes()
lookup = {}
for i, cells in enumerate(pool):
    for shape in orientations(cells):
        lookup[shape] = i
partners = [set() for _ in pool]
started = time.monotonic()
tested = 0
for i, cells in enumerate(pool):
    k = len(cells)
    for shape in orientations(cells):
        sw = max(x for x, y in shape) + 1
        sh = max(y for x, y in shape) + 1
        for width in range(sw, 11):
            for height in range(max(sh, width), (k + 10) // width + 1):
                if not 1 <= width * height - k <= 10:
                    continue
                full = {(x, y) for x in range(width) for y in range(height)}
                for x in range(width - sw + 1):
                    for y in range(height - sh + 1):
                        tested += 1
                        free = full - {(cx + x, cy + y) for cx, cy in shape}
                        minx = min(cx for cx, cy in free)
                        miny = min(cy for cx, cy in free)
                        normalized = tuple(sorted((cx - minx, cy - miny) for cx, cy in free))
                        if normalized in lookup:
                            partners[i].add(lookup[normalized])
    if i % 1000 == 0:
        print("enumerated", i, "seconds", time.monotonic() - started, flush=True)
print("partner degrees", collections.Counter(min(10, len(v)) for v in partners), flush=True)
for n in (100, 200, 500, 1000, 3000, 10000):
    frequencies = []
    for seed in range(20):
        rng = random.Random(173652 + seed)
        counts = collections.Counter(rng.randrange(len(pool)) for _ in range(n))
        pairs = []
        for a in sorted(counts, key=lambda a: len(partners[a])):
            for b in sorted(partners[a], key=lambda b: len(partners[b])):
                while counts[a] > 0 and counts[b] > int(a == b):
                    counts[a] -= 1
                    counts[b] -= 1
                    pairs.append((a, b))
        frequencies.append(2 * len(pairs) / n)
    print("pairable", n, sum(frequencies) / len(frequencies), min(frequencies), max(frequencies), flush=True)
print("tested", tested, "seconds", time.monotonic() - started, flush=True)
