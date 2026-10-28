import functools
import random
import subprocess
from pathlib import Path

from harness import encode, normalize, shifted, transform, validate


def exact_area(shapes):
    variants = [sorted({normalize(transform(shape, r, f))
                        for r in range(4) for f in range(2)}) for shape in shapes]
    lower = sum(map(len, shapes))
    upper = lower * 4
    for area in range(lower, upper + 1):
        for height in range(1, int(area ** 0.5) + 1):
            if area % height:
                continue
            width = area // height
            placements = []
            for choices in variants:
                masks = set()
                for shape in choices:
                    sw = max(x for x, y in shape) + 1
                    sh = max(y for x, y in shape) + 1
                    for ox in range(width - sw + 1):
                        for oy in range(height - sh + 1):
                            masks.add(sum(1 << ((y + oy) * width + x + ox)
                                          for x, y in shape))
                placements.append(tuple(masks))
            placements.sort(key=len)
            if not placements[0]:
                continue

            @functools.lru_cache(None)
            def search(index, occupied):
                if index == len(placements):
                    return True
                return any(not (occupied & mask) and search(index + 1, occupied | mask)
                           for mask in placements[index])

            if search(0, 0):
                return area
    raise AssertionError("reference shelf upper bound failed")


root = Path("/logs/agent/multiagent-collab/anti-herding/work-1")
rng = random.Random(202610261)
catalog = [
    ((0, 0),),
    ((0, 0), (1, 0)),
    ((0, 0), (1, 0), (2, 0)),
    ((0, 0), (1, 0), (1, 1)),
    ((0, 0), (1, 0), (2, 0), (2, 1)),
    ((0, 0), (1, 0), (1, 1), (2, 1)),
]
total_optimal = 0
for case in range(200):
    shapes = [shifted(rng.choice(catalog), rng) for _ in range(rng.randint(1, 4))]
    while sum(map(len, shapes)) > 10:
        shapes.pop()
    optimum = exact_area(shapes)
    output = subprocess.run([str(root / "frontier_open")], input=encode(shapes),
                            text=True, capture_output=True, check=True).stdout
    density, width, height = validate(shapes, output)
    if width * height < optimum:
        raise AssertionError("heuristic beats exhaustive optimum: transform mismatch")
    total_optimal += optimum
print(f"200 exact-reference comparisons valid; summed optimal areas={total_optimal}")
