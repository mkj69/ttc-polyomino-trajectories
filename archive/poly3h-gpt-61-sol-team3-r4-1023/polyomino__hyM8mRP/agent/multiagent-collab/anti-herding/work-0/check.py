import argparse
import random
import subprocess
import time


def make_input(rng, n, mode):
    shapes = []
    for i in range(n):
        k = rng.randint(1, 10) if mode == "mixed" else 10
        if mode == "line":
            shape = {(x, 0) for x in range(k)}
        elif mode == "hole":
            shape = {(x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1)}
            if i % 2:
                shape.add((3, 1))
        else:
            shape = {(0, 0)}
            while len(shape) < k:
                x, y = rng.choice(tuple(shape))
                dx, dy = rng.choice(((0, 1), (0, -1), (1, 0), (-1, 0)))
                shape.add((x + dx, y + dy))
        ox, oy = rng.randint(-100000, 100000), rng.randint(-100000, 100000)
        shapes.append([(x + ox, y + oy) for x, y in shape])
    data = [str(n)]
    for shape in shapes:
        data.append(str(len(shape)))
        data.extend(f"{x} {y}" for x, y in shape)
    return shapes, "\n".join(data) + "\n"


def validate(shapes, output):
    words = list(map(int, output.split()))
    assert len(words) == 2 + 4 * len(shapes), "wrong output length"
    w, h = words[:2]
    assert w > 0 and h > 0, "nonpositive rectangle"
    used = set()
    for i, shape in enumerate(shapes):
        tx, ty, r, f = words[2 + 4 * i:6 + 4 * i]
        assert 0 <= r < 4 and f in (0, 1), "invalid transform"
        for x, y in shape:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            x, y = x + tx, y + ty
            assert 0 <= x < w and 0 <= y < h, ("out of bounds", i, x, y, w, h)
            assert (x, y) not in used, ("overlap", i, x, y)
            used.add((x, y))
    return len(used) / (w * h)


def reference(shapes):
    xoff, height = 0, 0
    placements = []
    for shape in shapes:
        minx = min(x for x, y in shape)
        miny = min(y for x, y in shape)
        width = max(x for x, y in shape) - minx + 1
        height = max(height, max(y for x, y in shape) - miny + 1)
        placements.append(f"{xoff - minx} {-miny} 0 0")
        xoff += width
    return f"{xoff} {height}\n" + "\n".join(placements)


parser = argparse.ArgumentParser()
parser.add_argument("binary")
parser.add_argument("--cases", type=int, default=100)
parser.add_argument("--large", type=int, default=0)
parser.add_argument("--mode", default="mixed")
parser.add_argument("--seed", type=int, default=12345)
args = parser.parse_args()
rng = random.Random(args.seed)
densities = []
elapsed = []
for case in range(args.cases):
    n = args.large if args.large else (1 if case == 0 else rng.randint(2, 150))
    shapes, data = make_input(rng, n, args.mode)
    validate(shapes, reference(shapes))
    start = time.monotonic()
    result = subprocess.run([args.binary], input=data, text=True, capture_output=True, timeout=30)
    elapsed.append(time.monotonic() - start)
    assert result.returncode == 0, (result.returncode, result.stderr)
    density = validate(shapes, result.stdout)
    densities.append(density)
    if args.large or args.cases <= 10:
        print(f"case={case} n={n} mode={args.mode} density={density:.6f} time={elapsed[-1]:.3f}", flush=True)
print(f"PASS cases={args.cases} density_mean={sum(densities)/len(densities):.6f} "
      f"density_min={min(densities):.6f} runtime_max={max(elapsed):.3f} "
      f"runtime_total={sum(elapsed):.3f}", flush=True)
