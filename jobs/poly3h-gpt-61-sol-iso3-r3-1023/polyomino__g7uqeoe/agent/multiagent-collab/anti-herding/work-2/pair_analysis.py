import pickle
import random
import time
from collections import Counter


def normalize(cells):
    x0 = min(x for x, y in cells)
    y0 = min(y for x, y in cells)
    return tuple(sorted((x - x0, y - y0) for x, y in cells))


def variants(cells):
    result = set()
    for reflection in range(2):
        for rotation in range(4):
            transformed = []
            for x, y in cells:
                if reflection:
                    x = -x
                for _ in range(rotation):
                    x, y = y, -x
                transformed.append((x, y))
            result.add(normalize(transformed))
    return result


def main():
    start = time.monotonic()
    with open("/logs/agent/multiagent-collab/anti-herding/work-0/free-polyominoes.pkl", "rb") as handle:
        catalog = pickle.load(handle)
    all_variants = [variants(shape) for shape in catalog]
    lookup = {shape: i for i, choices in enumerate(all_variants) for shape in choices}
    neighbors = [set() for _ in catalog]
    dimensions = ((4, 5), (2, 10), (3, 6), (2, 9), (4, 4), (3, 5), (2, 8), (2, 7), (3, 4))
    for i, choices in enumerate(all_variants):
        for width, height in dimensions:
            other_area = width * height - len(catalog[i])
            if not 1 <= other_area <= 10:
                continue
            full = (1 << (width * height)) - 1
            for shape in choices:
                sx = max(x for x, y in shape) + 1
                sy = max(y for x, y in shape) + 1
                if sx > width or sy > height:
                    continue
                mask = sum(1 << (y * width + x) for x, y in shape)
                for y in range(height - sy + 1):
                    for x in range(width - sx + 1):
                        complement = full ^ (mask << (y * width + x))
                        cells = [(at % width, at // width) for at in range(width * height)
                                 if complement >> at & 1]
                        j = lookup.get(normalize(cells))
                        if j is not None:
                            neighbors[i].add(j)
                            neighbors[j].add(i)
    print("catalog", len(catalog), "pairable_types", sum(bool(x) for x in neighbors),
          "directed_edges", sum(map(len, neighbors)), "seconds", time.monotonic() - start, flush=True)
    for n in (100, 220, 1000, 10000):
        fractions = []
        for seed in range(20):
            rng = random.Random(83214 + seed)
            original = Counter(rng.randrange(len(catalog)) for _ in range(n))
            counts = original.copy()
            paired_area = 0
            order = list(counts)
            order.sort(key=lambda i: len(neighbors[i]))
            for i in order:
                while counts[i]:
                    options = [j for j in neighbors[i] if counts[j] > int(j == i)]
                    if not options:
                        break
                    j = min(options, key=lambda j: len(neighbors[j]))
                    counts[i] -= 1
                    counts[j] -= 1
                    paired_area += len(catalog[i]) + len(catalog[j])
            area = sum(len(catalog[i]) * count for i, count in original.items())
            fractions.append(paired_area / area)
        print("n", n, "mean_tiled_area_fraction", sum(fractions) / len(fractions), flush=True)


if __name__ == "__main__":
    main()
