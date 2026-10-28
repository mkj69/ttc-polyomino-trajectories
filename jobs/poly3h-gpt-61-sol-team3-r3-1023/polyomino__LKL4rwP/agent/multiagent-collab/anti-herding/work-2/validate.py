import argparse
import random
import shlex
import subprocess
import time
from pathlib import Path


ROOT = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def transformed(shape, rotation, reflection):
    cells = []
    for x, y in shape:
        if reflection:
            x = -x
        for _ in range(rotation):
            x, y = y, -x
        cells.append((x, y))
    return cells


def validate(shapes, text):
    lines = text.splitlines()
    assert len(lines) == len(shapes) + 1, ("line count", len(lines), len(shapes))
    width, height = map(int, lines[0].split())
    assert width > 0 and height > 0, ("rectangle", width, height)
    occupied = set()
    for index, (shape, line) in enumerate(zip(shapes, lines[1:])):
        tx, ty, rotation, reflection = map(int, line.split())
        assert 0 <= rotation < 4 and 0 <= reflection < 2, ("transform", index)
        for x, y in transformed(shape, rotation, reflection):
            x += tx
            y += ty
            assert 0 <= x < width and 0 <= y < height, ("bounds", index, x, y, width, height)
            assert (x, y) not in occupied, ("overlap", index, x, y)
            occupied.add((x, y))
    return len(occupied) / (width * height), width, height


def normalize(shape):
    minx = min(x for x, _ in shape)
    miny = min(y for _, y in shape)
    return tuple(sorted((x - minx, y - miny) for x, y in shape))


def brute_force(shapes):
    all_orientations = []
    for shape in shapes:
        all_orientations.append(set(normalize(transformed(shape, r, f)) for r in range(4) for f in range(2)))
    total = sum(map(len, shapes))
    upper = sum(max(x for x, _ in normalize(s)) + 1 for s in shapes)
    upper *= max(max(y for _, y in normalize(s)) + 1 for s in shapes)
    order = sorted(range(len(shapes)), key=lambda i: -len(shapes[i]))
    for area in range(total, upper + 1):
        for height in range(1, area + 1):
            if area % height:
                continue
            width = area // height
            placements = []
            for i in order:
                options = []
                for shape in all_orientations[i]:
                    sw = max(x for x, _ in shape) + 1
                    sh = max(y for _, y in shape) + 1
                    for y in range(height - sh + 1):
                        for x in range(width - sw + 1):
                            options.append(sum(1 << ((cy + y) * width + cx + x) for cx, cy in shape))
                placements.append(options)

            def dfs(index, used):
                if index == len(order):
                    return True
                return any(not (mask & used) and dfs(index + 1, used | mask) for mask in placements[index])

            if dfs(0, 0):
                return area
    raise AssertionError("reference search failed")


def encode(shapes):
    lines = [str(len(shapes))]
    for shape in shapes:
        lines.append(str(len(shape)))
        lines.extend(f"{x} {y}" for x, y in shape)
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--quick", action="store_true")
    parser.add_argument("--seed", type=int, default=192867)
    parser.add_argument("--cases", type=int, default=300)
    parser.add_argument("--options", default="")
    parser.add_argument("--sizes", default="")
    args = parser.parse_args()
    rng = random.Random(args.seed)
    raw = (ROOT / "shapes.txt").read_text().splitlines()
    catalog = []
    for line in raw[1:]:
        values = list(map(int, line.split()))
        catalog.append(list(zip(values[1::2], values[2::2])))
    metrics = []
    for case in range(args.cases):
        if case < 20:
            n = 1 + case % 4
            choices = catalog[:9]
        else:
            sizes = list(map(int, args.sizes.split(","))) if args.sizes else ([100, 101, 300, 1000, 3000, 10000] if not args.quick else [100, 101, 300])
            n = rng.choice(sizes)
            choices = catalog
        shapes = []
        for _ in range(n):
            shape = list(rng.choice(choices))
            r, f = rng.randrange(4), rng.randrange(2)
            tx, ty = rng.randrange(-1000, 1001), rng.randrange(-1000, 1001)
            shape = [(x + tx, y + ty) for x, y in transformed(shape, r, f)]
            shapes.append(shape)
        started = time.monotonic()
        run = subprocess.run([args.binary] + shlex.split(args.options), input=encode(shapes), text=True, capture_output=True, timeout=10)
        elapsed = time.monotonic() - started
        assert run.returncode == 0, ("crash", case, run.returncode, run.stderr)
        try:
            density, width, height = validate(shapes, run.stdout)
            if case < 20:
                optimum = brute_force(shapes)
                assert width * height >= optimum, ("reference", case, width, height, optimum)
        except AssertionError:
            (ROOT / "failing.in").write_text(encode(shapes))
            (ROOT / "failing.out").write_text(run.stdout)
            raise
        metrics.append((n, density, elapsed))
        if case < 20 or case % 20 == 0:
            print(f"case={case} n={n} density={density:.6f} elapsed={elapsed:.4f}", flush=True)
    for n in sorted(set(m[0] for m in metrics)):
        vals = [m for m in metrics if m[0] == n]
        print(f"SUMMARY n={n} cases={len(vals)} density={sum(v[1] for v in vals)/len(vals):.6f} max_seconds={max(v[2] for v in vals):.4f}")


if __name__ == "__main__":
    main()
