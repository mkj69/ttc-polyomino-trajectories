import argparse
import random
import subprocess
import time
from pathlib import Path


def shape(rng, k):
    cells = {(0, 0)}
    while len(cells) < k:
        x, y = rng.choice(tuple(cells))
        dx, dy = rng.choice(((0, 1), (1, 0), (0, -1), (-1, 0)))
        cells.add((x + dx, y + dy))
    ox, oy = rng.randint(-100, 100), rng.randint(-100, 100)
    return [(x + ox, y + oy) for x, y in cells]


def encode(pieces):
    rows = [str(len(pieces))]
    for piece in pieces:
        rows.append(str(len(piece)))
        rows.extend(f"{x} {y}" for x, y in piece)
    return "\n".join(rows) + "\n"


def validate(pieces, output):
    rows = output.splitlines()
    assert len(rows) == len(pieces) + 1, (len(rows), len(pieces))
    w, h = map(int, rows[0].split())
    assert w > 0 and h > 0
    seen = set()
    for i, (piece, row) in enumerate(zip(pieces, rows[1:])):
        tx, ty, r, f = map(int, row.split())
        assert 0 <= r < 4 and 0 <= f < 2
        for x, y in piece:
            x = (-1 if f else 1) * x
            x, y = ((x, y), (y, -x), (-x, -y), (-y, x))[r]
            x, y = x + tx, y + ty
            assert 0 <= x < w and 0 <= y < h, (i, x, y, w, h)
            assert (x, y) not in seen, (i, x, y)
            seen.add((x, y))
    return w * h


def reference(pieces):
    xoff = 0
    height = 1
    placements = []
    for piece in pieces:
        minx, miny = min(x for x, y in piece), min(y for x, y in piece)
        width = max(x for x, y in piece) - minx + 1
        height = max(height, max(y for x, y in piece) - miny + 1)
        placements.append(f"{xoff - minx} {-miny} 0 0")
        xoff += width
    return f"{xoff} {height}\n" + "\n".join(placements) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--count", type=int, default=200)
    parser.add_argument("--large", type=int, default=0)
    parser.add_argument("--save")
    parser.add_argument("--seed", type=int, default=7239)
    parser.add_argument("args", nargs="*")
    options = parser.parse_args()
    rng = random.Random(options.seed)
    densities, runtimes = [], []
    for it in range(options.count):
        n = options.large or rng.randint(1, 40)
        pieces = [shape(rng, rng.randint(1, 10)) for _ in range(n)]
        data = encode(pieces)
        if options.save:
            Path(options.save).write_text(data)
        begin = time.monotonic()
        proc = subprocess.run([options.binary, *options.args], input=data,
                              text=True, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, check=True, timeout=30)
        runtimes.append(time.monotonic() - begin)
        area = validate(pieces, proc.stdout)
        validate(pieces, reference(pieces))
        densities.append(sum(map(len, pieces)) / area)
    print(f"cases={options.count} density_min={min(densities):.6f} "
          f"density_avg={sum(densities)/len(densities):.6f} "
          f"time_max={max(runtimes):.6f} "
          f"time_total={sum(runtimes):.6f}")


if __name__ == "__main__":
    main()
