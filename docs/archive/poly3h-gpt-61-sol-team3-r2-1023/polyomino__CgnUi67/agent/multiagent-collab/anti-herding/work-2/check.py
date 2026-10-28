import argparse
import random
import subprocess
import time


def grow(rng, size):
    cells = {(0, 0)}
    while len(cells) < size:
        x, y = rng.choice(tuple(cells))
        dx, dy = rng.choice([(0, 1), (1, 0), (0, -1), (-1, 0)])
        cells.add((x + dx, y + dy))
    return sorted(cells)


def generate(seed, n, mode):
    rng = random.Random(seed)
    pieces = []
    for _ in range(n):
        if mode == "monomino":
            cells = [(0, 0)]
        elif mode == "line":
            cells = [(j, 0) for j in range(10)]
        elif mode == "hole":
            cells = [(x, y) for x in range(3) for y in range(3)
                     if (x, y) != (1, 1)]
        elif mode == "10":
            cells = grow(rng, 10)
        else:
            size = rng.choices(range(1, 11), weights=[1, 1, 2, 5, 12, 35,
                                                    108, 369, 1285, 4655])[0]
            cells = grow(rng, size)
        dx, dy = rng.randint(-10000, 10000), rng.randint(-10000, 10000)
        pieces.append([(x + dx, y + dy) for x, y in cells])
    return pieces


def validate(pieces, output):
    rows = [list(map(int, s.split())) for s in output.splitlines()]
    assert len(rows) == len(pieces) + 1, (len(rows), len(pieces) + 1)
    assert len(rows[0]) == 2
    w, h = rows[0]
    assert 0 < w and 0 < h
    seen = set()
    for i, (piece, row) in enumerate(zip(pieces, rows[1:])):
        assert len(row) == 4
        dx, dy, r, f = row
        assert 0 <= r <= 3 and 0 <= f <= 1
        for x, y in piece:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            x += dx
            y += dy
            assert 0 <= x < w and 0 <= y < h, (i, x, y, w, h)
            assert (x, y) not in seen, (i, x, y)
            seen.add((x, y))
    return len(seen) / (w * h)


def run(exe, pieces):
    data = str(len(pieces)) + "\n"
    for p in pieces:
        data += str(len(p)) + "\n" + "".join(f"{x} {y}\n" for x, y in p)
    t = time.monotonic()
    result = subprocess.run([exe], input=data, capture_output=True, text=True,
                            timeout=20, check=True)
    elapsed = time.monotonic() - t
    return validate(pieces, result.stdout), elapsed, result.stderr


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("exe")
    parser.add_argument("--n", type=int, default=100)
    parser.add_argument("--seeds", type=int, default=3)
    parser.add_argument("--mode", default="mixed")
    args = parser.parse_args()
    for seed in range(args.seeds):
        d, t, err = run(args.exe, generate(seed, args.n, args.mode))
        print(f"n={args.n} mode={args.mode} seed={seed} density={d:.5f} "
              f"time={t:.4f}\n{err}", flush=True)
