import argparse
import random
import shlex
import subprocess
import time


def generate(n, seed, fixed_size=0, offset=True):
    rng = random.Random(seed)
    pieces = []
    for _ in range(n):
        k = fixed_size or rng.choices(range(1, 11), [1, 1, 2, 5, 12, 35, 108, 369, 1285, 4655])[0]
        cells = {(0, 0)}
        while len(cells) < k:
            x, y = rng.choice(tuple(cells))
            dx, dy = rng.choice([(0, 1), (0, -1), (1, 0), (-1, 0)])
            cells.add((x + dx, y + dy))
        ox, oy = (rng.randint(-1000, 1000), rng.randint(-1000, 1000)) if offset else (0, 0)
        pieces.append([(x + ox, y + oy) for x, y in cells])
    return pieces


def encode(pieces):
    return str(len(pieces)) + "\n" + "".join(
        str(len(p)) + "\n" + "".join(f"{x} {y}\n" for x, y in p) for p in pieces
    )

def decode(text):
    values = list(map(int, text.split()))
    pieces = []
    at = 1
    for _ in range(values[0]):
        k = values[at]
        at += 1
        pieces.append(list(zip(values[at:at + 2 * k:2], values[at + 1:at + 2 * k:2])))
        at += 2 * k
    return pieces


def validate(pieces, text):
    vals = list(map(int, text.split()))
    assert len(vals) == 2 + 4 * len(pieces), ("output length", len(vals))
    width, height = vals[:2]
    assert width >= 0 and height >= 0
    occupied = set()
    for i, p in enumerate(pieces):
        tx, ty, r, f = vals[2 + 4 * i:6 + 4 * i]
        assert 0 <= r < 4 and 0 <= f < 2
        for x, y in p:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            x, y = x + tx, y + ty
            assert 0 <= x < width and 0 <= y < height, ("bounds", i, (x, y), (width, height))
            assert (x, y) not in occupied, ("overlap", i, (x, y))
            occupied.add((x, y))
    return len(occupied) / (width * height) if pieces else 1.0


def reference(pieces):
    if not pieces:
        return "0 0\n"
    xoffset = height = 0
    placements = []
    for p in pieces:
        minx = min(x for x, y in p)
        miny = min(y for x, y in p)
        maxx = max(x for x, y in p)
        maxy = max(y for x, y in p)
        placements.append((xoffset - minx, -miny, 0, 0))
        xoffset += maxx - minx + 1
        height = max(height, maxy - miny + 1)
    return f"{xoffset} {height}\n" + "".join(" ".join(map(str, p)) + "\n" for p in placements)


def run(binary, pieces):
    start = time.monotonic()
    result = subprocess.run(shlex.split(binary), input=encode(pieces), text=True, capture_output=True, timeout=20)
    assert result.returncode == 0, result.stderr
    elapsed = time.monotonic() - start
    density = validate(pieces, result.stdout)
    assert validate(pieces, reference(pieces)) <= 1.0
    return density, elapsed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--trials", type=int, default=200)
    parser.add_argument("--large", type=int, default=10000)
    parser.add_argument("--save", default="")
    args = parser.parse_args()
    for seed in range(args.trials):
        pieces = generate(seed % 35 + 1, seed, fixed_size=seed % 11)
        run(args.binary, pieces)
    print(f"validated {args.trials} random small cases")
    for n in [100, 1000, args.large]:
        pieces = generate(n, 58173 + n)
        density, elapsed = run(args.binary, pieces)
        print(f"n={n} density={density:.6f} time={elapsed:.3f}s")
        if args.save and n == args.large:
            with open(args.save, "w") as f:
                f.write(encode(pieces))
    cases = [
        [],
        [[(0, 0)]],
        [[(j, 0) for j in range(10)] for _ in range(100)],
        [[(j % 3, j // 3) for j in range(9) if j != 4] for _ in range(100)],
        [[(0, 0)]] * 10000,
    ]
    for i, pieces in enumerate(cases):
        density, elapsed = run(args.binary, pieces)
        print(f"edge={i} density={density:.6f} time={elapsed:.3f}s")


if __name__ == "__main__":
    main()
