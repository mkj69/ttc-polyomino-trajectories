import random
import subprocess
import sys
import time

from selftest import encode, validate
from uniformtest import parse


def reference(pieces):
    width = 0
    placements = []
    height = 0
    for cells in pieces:
        minx = min(x for x, _ in cells)
        miny = min(y for _, y in cells)
        maxx = max(x for x, _ in cells)
        maxy = max(y for _, y in cells)
        placements.append(f"{width - minx} {-miny} 0 0")
        width += maxx - minx + 1
        height = max(height, maxy - miny + 1)
    return f"{width} {height}\n" + "\n".join(placements) + "\n"


def transformed(cells, rng):
    f, r = rng.randrange(2), rng.randrange(4)
    ox, oy = rng.choice([-1000000000, 1000000000]), rng.choice([-1000000000, 1000000000])
    result = []
    for x, y in cells:
        if f:
            x = -x
        for _ in range(r):
            x, y = y, -x
        result.append((x + ox, y + oy))
    rng.shuffle(result)
    return result


def main():
    rng = random.Random(562173)
    pool = parse(subprocess.run(["./generator", "0"], capture_output=True,
                               text=True, check=True).stdout)
    ring = [(x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1)]
    bar = [(i, 0) for i in range(10)]
    square = [(x, y) for x in range(3) for y in range(3)]
    cases = [
        ("single-monomino", [[(1000000000, -1000000000)]]),
        ("single-hole", [ring]),
        ("single-chiral", [[(0, 0), (1, 0), (2, 0), (2, 1), (2, 2), (3, 2), (3, 3)]]),
        ("monomino-100", [[(0, 0)]] * 100),
        ("bars-100", [transformed(bar, rng) for _ in range(100)]),
        ("rings-100", [transformed(ring, rng) for _ in range(100)]),
        ("squares-100", [transformed(square, rng) for _ in range(100)]),
        ("ring-hole-fillers", [transformed(ring, rng) for _ in range(100)] +
         [transformed([(0, 0)], rng) for _ in range(100)]),
        ("monomino-10000", [[(0, 0)]] * 10000),
        ("bars-10000", [transformed(bar, rng) for _ in range(10000)]),
        ("rings-10000", [transformed(ring, rng) for _ in range(10000)]),
    ]
    for n in [100, 101, 150, 300, 600, 601, 1000, 1001, 10000]:
        cases.append((f"catalogue-shift-{n}",
                      [transformed(rng.choice(pool), rng) for _ in range(n)]))
    for label, pieces in cases:
        validate(pieces, reference(pieces))
        begin = time.monotonic()
        result = subprocess.run(sys.argv[1:], input=encode(pieces), text=True,
                                capture_output=True, timeout=8, check=True)
        density = validate(pieces, result.stdout)
        print(f"{label}: density={density:.6f} seconds={time.monotonic()-begin:.3f}",
              flush=True)
    print(f"validated={len(cases)} including independent shelf reference")


if __name__ == "__main__":
    main()
