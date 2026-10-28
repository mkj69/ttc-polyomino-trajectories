import argparse
import random
import subprocess
import time

CATALOG = "/logs/agent/multiagent-collab/anti-herding/work-0/shapes.txt"


def rotate(x, y, r, f):
    if f:
        x = -x
    for _ in range(r):
        x, y = y, -x
    return x, y


def generate(n, seed, style):
    rng = random.Random(seed)
    result = []
    if style == "uniform":
        with open(CATALOG) as source:
            tokens = iter(map(int, source.read().split()))
            catalog = []
            for _ in range(next(tokens)):
                catalog.append([(next(tokens), next(tokens))
                                for _ in range(next(tokens))])
        for _ in range(n):
            r, f = rng.randrange(4), rng.randrange(2)
            ox, oy = rng.randint(-1000, 1000), rng.randint(-1000, 1000)
            result.append([(a + ox, b + oy)
                           for a, b in
                           (rotate(x, y, r, f)
                            for x, y in rng.choice(catalog))])
        return result
    for _ in range(n):
        k = rng.randint(1, 10) if style == "mixed" else 10
        cells = {(0, 0)}
        while len(cells) < k:
            x, y = rng.choice(tuple(cells))
            dx, dy = rng.choice(((0, 1), (0, -1), (1, 0), (-1, 0)))
            cells.add((x + dx, y + dy))
        ox, oy = rng.randint(-1000, 1000), rng.randint(-1000, 1000)
        result.append([(x + ox, y + oy) for x, y in cells])
    return result


def validate(pieces, output):
    values = list(map(int, output.split()))
    assert len(values) == 2 + 4 * len(pieces), len(values)
    w, h = values[:2]
    assert w > 0 and h > 0
    occupied = set()
    for i, cells in enumerate(pieces):
        x, y, r, f = values[2 + i * 4: 6 + i * 4]
        assert 0 <= r <= 3 and 0 <= f <= 1
        for a, b in cells:
            a, b = rotate(a, b, r, f)
            a, b = a + x, b + y
            assert 0 <= a < w and 0 <= b < h, (i, a, b, w, h)
            assert (a, b) not in occupied, (i, a, b)
            occupied.add((a, b))
    return len(occupied) / (w * h), w, h


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--n", type=int, default=100)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--cases", type=int, default=1)
    p.add_argument("--style", default="large")
    p.add_argument("--exe", default="/app/solution")
    p.add_argument("args", nargs="*")
    opt = p.parse_args()
    for seed in range(opt.seed, opt.seed + opt.cases):
        pieces = generate(opt.n, seed, opt.style)
        text = str(len(pieces)) + "\n"
        for cells in pieces:
            text += str(len(cells)) + "\n"
            text += "".join(f"{x} {y}\n" for x, y in cells)
        start = time.monotonic()
        run = subprocess.run([opt.exe] + opt.args, input=text, text=True,
                             capture_output=True, check=True, timeout=15)
        elapsed = time.monotonic() - start
        density, w, h = validate(pieces, run.stdout)
        print(f"n={opt.n} seed={seed} density={density:.6f} "
              f"width={w} height={h} seconds={elapsed:.4f}", flush=True)


if __name__ == "__main__":
    main()
