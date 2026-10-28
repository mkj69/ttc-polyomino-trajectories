import random
import sys
from pathlib import Path

from check import run


def read_catalogue():
    values = list(map(int, Path(__file__).with_name("catalogue.txt").read_text().split()))
    shapes = []
    at = 1
    for _ in range(values[0]):
        k = values[at]
        at += 1
        shapes.append([(values[j], values[j + 1]) for j in range(at, at + 2 * k, 2)])
        at += 2 * k
    assert at == len(values)
    assert len(shapes) == 6473
    return shapes


if __name__ == "__main__":
    catalogue = read_catalogue()
    rng = random.Random(7123981)
    cases = []
    for n in (100, 300, 1000, 3000, 10000):
        cases.append([rng.choice(catalogue) for _ in range(n)])
    if len(sys.argv) > 2:
        configs = [tuple(map(int, sys.argv[2:]))]
    else:
        configs = [(0, s) for s in (16, 48, 128, 384)]
        configs += [(w, 128) for w in (24, 32, 48, 64)]
    for width, samples, *rest in configs:
        results = []
        for pieces in cases:
            w = width or min(64, max(10, int(sum(map(len, pieces)) ** 0.5)))
            results.append(run(sys.argv[1], pieces, (w, samples, *rest)))
        print(f"width={width} samples={samples} extra={rest}: "
              + " ".join(f"{density:.5f}/{duration:.3f}s" for density, duration in results),
              flush=True)
