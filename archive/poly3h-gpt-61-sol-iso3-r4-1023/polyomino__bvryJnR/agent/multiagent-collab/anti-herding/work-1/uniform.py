import argparse
import random
import subprocess
import time


def transforms(shape):
    variants = set()
    for f in range(2):
        for r in range(4):
            cells = []
            for x, y in shape:
                if f:
                    x = -x
                for _ in range(r):
                    x, y = y, -x
                cells.append((x, y))
            min_x = min(x for x, y in cells)
            min_y = min(y for x, y in cells)
            variants.add(tuple(sorted((x - min_x, y - min_y) for x, y in cells)))
    return variants


def exhaustive_area(shapes):
    total = sum(map(len, shapes))
    variants = [transforms(s) for s in shapes]
    order = sorted(range(len(shapes)), key=lambda i: -len(shapes[i]))
    for area in range(total, total * 3 + 1):
        for h in range(1, area + 1):
            if area % h:
                continue
            w = area // h
            masks = []
            for i in order:
                choices = []
                for form in variants[i]:
                    fw = max(x for x, y in form) + 1
                    fh = max(y for x, y in form) + 1
                    for y0 in range(h - fh + 1):
                        for x0 in range(w - fw + 1):
                            choices.append(sum(1 << ((y + y0) * w + x + x0) for x, y in form))
                masks.append(choices)
            def dfs(i, occupied):
                if i == len(order):
                    return True
                for mask in masks[i]:
                    if not mask & occupied and dfs(i + 1, occupied | mask):
                        return True
                return False
            if dfs(0, 0):
                return area
    raise AssertionError("reference failed")


def generate(rng, n, mode="random"):
    shapes = []
    for _ in range(n):
        k = rng.randint(1, 10)
        if mode == "tiny":
            k = rng.randint(1, 4)
        if mode == "large":
            k = 10
        cells = {(0, 0)}
        while len(cells) < k:
            x, y = rng.choice(tuple(cells))
            dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
            cells.add((x + dx, y + dy))
        ox, oy = rng.randint(-1000000, 1000000), rng.randint(-1000000, 1000000)
        shapes.append([(x + ox, y + oy) for x, y in cells])
    return shapes


def catalogue():
    forms = {((0, 0),)}
    result = list(forms)
    for size in range(2, 11):
        expanded = set()
        for shape in forms:
            cells = set(shape)
            border = set()
            for x, y in shape:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    if (x + dx, y + dy) not in cells:
                        border.add((x + dx, y + dy))
            for cell in border:
                expanded.add(min(transforms(shape + (cell,))))
        forms = expanded
        result.extend(forms)
        print(f"catalogue size={size} count={len(forms)}", flush=True)
    return result


def run(binary, shapes):
    text = str(len(shapes)) + "\n"
    for shape in shapes:
        text += str(len(shape)) + "\n" + "".join(f"{x} {y}\n" for x, y in shape)
    start = time.monotonic()
    proc = subprocess.run([binary], input=text, text=True, capture_output=True, check=True, timeout=10)
    elapsed = time.monotonic() - start
    values = list(map(int, proc.stdout.split()))
    assert len(values) == 2 + 4 * len(shapes), (len(values), len(shapes))
    w, h = values[:2]
    assert w > 0 and h > 0
    occupied = set()
    for i, shape in enumerate(shapes):
        tx, ty, r, f = values[2 + 4 * i:6 + 4 * i]
        assert 0 <= r < 4 and 0 <= f < 2
        for x, y in shape:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            x, y = x + tx, y + ty
            assert 0 <= x < w and 0 <= y < h, (i, x, y, w, h)
            assert (x, y) not in occupied, (i, x, y)
            occupied.add((x, y))
    return len(occupied) / (w * h), elapsed, w * h


parser = argparse.ArgumentParser()
parser.add_argument("binary")
parser.add_argument("--seed", type=int, default=9173)
parser.add_argument("--quick", action="store_true")
parser.add_argument("--uniform", action="store_true")
parser.add_argument("--bench", action="store_true")
args = parser.parse_args()
rng = random.Random(args.seed)
count = 0 if args.bench else (50 if args.quick else 300)
for case in range(count):
    shapes = generate(rng, rng.randint(1, 4), "tiny")
    density, elapsed, area = run(args.binary, shapes)
    optimum = exhaustive_area(shapes)
    assert area >= optimum, (area, optimum)
print(f"small cross-validation: {count} valid outputs vs exhaustive reference")
if args.uniform:
    all_shapes = catalogue()
    for n in (100, 150, 300, 1000, 3000, 10000):
        measures = [run(args.binary, [rng.choice(all_shapes) for _ in range(n)]) for _ in range(3)]
        print(f"uniform n={n} density={sum(m[0] for m in measures)/len(measures):.5f} time={max(m[1] for m in measures):.4f}s", flush=True)
    raise SystemExit
for n in (100, 1000, 10000):
    for mode in ("random", "large"):
        measures = [run(args.binary, generate(rng, n, mode)) for _ in range(2)]
        print(f"n={n} mode={mode} density={sum(m[0] for m in measures)/len(measures):.5f} time={max(m[1] for m in measures):.4f}s")
