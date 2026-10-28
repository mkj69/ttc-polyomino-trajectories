import random
import subprocess
import sys
import time


def make_piece(k, rng):
    cells = {(0, 0)}
    while len(cells) < k:
        x, y = rng.choice(tuple(cells))
        dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
        cells.add((x + dx, y + dy))
    ox, oy = rng.randrange(-100, 101), rng.randrange(-100, 101)
    return [(x + ox, y + oy) for x, y in cells]


def validate(pieces, output):
    tokens = list(map(int, output.split()))
    assert len(tokens) == 2 + 4 * len(pieces), len(tokens)
    width, height = tokens[:2]
    assert width > 0 and height > 0
    used = set()
    for index, piece in enumerate(pieces):
        tx, ty, r, f = tokens[2 + 4 * index:6 + 4 * index]
        assert r in range(4) and f in (0, 1)
        for x, y in piece:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            x, y = x + tx, y + ty
            assert 0 <= x < width and 0 <= y < height, (index, x, y, width, height)
            assert (x, y) not in used, (index, x, y)
            used.add((x, y))
    return len(used) / (width * height)


def run(binary, pieces, args=()):
    data = [str(len(pieces))]
    for piece in pieces:
        data.append(str(len(piece)))
        data.extend(f"{x} {y}" for x, y in piece)
    start = time.monotonic()
    result = subprocess.run([binary, *map(str, args)], input="\n".join(data) + "\n",
                            text=True, capture_output=True, check=True, timeout=5)
    return validate(pieces, result.stdout), time.monotonic() - start


if __name__ == "__main__":
    binary = sys.argv[1]
    rng = random.Random(1234567)
    densities = []
    worst = 0
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    for case in range(count):
        n = rng.randrange(1, 130) if case < count - 2 else 10000
        mode = case % 7
        pieces = [make_piece(1 if mode == 0 else 10 if mode == 1 else rng.randrange(1, 11), rng)
                  for _ in range(n)]
        density, duration = run(binary, pieces)
        densities.append(density)
        worst = max(worst, duration)
    print(f"valid={count} mean_density={sum(densities) / count:.6f} "
          f"min_density={min(densities):.6f} max_seconds={worst:.3f}")
