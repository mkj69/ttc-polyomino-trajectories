import random
import subprocess
import sys
import time


def generate(n, rng, mode):
    pieces = []
    for i in range(n):
        k = rng.randint(1, 10) if mode != "large" else 10
        cells = {(0, 0)}
        while len(cells) < k:
            x, y = rng.choice(tuple(cells))
            dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
            cells.add((x + dx, y + dy))
        ox, oy = rng.randint(-100, 100), rng.randint(-100, 100)
        pieces.append([(x + ox, y + oy) for x, y in cells])
    return pieces


def encode(pieces):
    lines = [str(len(pieces))]
    for cells in pieces:
        lines.append(str(len(cells)))
        lines.extend(f"{x} {y}" for x, y in cells)
    return "\n".join(lines) + "\n"


def validate(pieces, output):
    values = list(map(int, output.split()))
    assert len(values) == 2 + 4 * len(pieces), len(values)
    w, h = values[:2]
    assert w > 0 and h > 0, (w, h)
    used = set()
    for i, cells in enumerate(pieces):
        tx, ty, r, f = values[2 + 4 * i:6 + 4 * i]
        assert 0 <= r < 4 and 0 <= f < 2
        for x, y in cells:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            p = x + tx, y + ty
            assert 0 <= p[0] < w and 0 <= p[1] < h, (i, p, w, h)
            assert p not in used, (i, p)
            used.add(p)
    return len(used) / (w * h)


def main():
    exe = sys.argv[1]
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    rng = random.Random(731442)
    density = []
    start = time.monotonic()
    for t in range(count):
        n = rng.randint(1, 160)
        if count == 1:
            n = 10000
        pieces = generate(n, rng, "large" if count == 1 else "mix")
        before = time.monotonic()
        result = subprocess.run([exe], input=encode(pieces), text=True,
                                capture_output=True, check=True, timeout=10)
        elapsed = time.monotonic() - before
        density.append(validate(pieces, result.stdout))
        if count == 1:
            print(f"stress n={n} time={elapsed:.3f}s density={density[-1]:.6f}")
    print(f"valid={count} mean_density={sum(density)/len(density):.6f}"
          f" total_time={time.monotonic()-start:.3f}s")


if __name__ == "__main__":
    main()
