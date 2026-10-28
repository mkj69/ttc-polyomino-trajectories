import argparse
import json
import random
import subprocess
import time
from pathlib import Path


ROOT = Path("/logs/agent/multiagent-collab/anti-herding/work-1")


def normalize(cells):
    lo_x = min(x for x, y in cells)
    lo_y = min(y for x, y in cells)
    return tuple(sorted((x - lo_x, y - lo_y) for x, y in cells))


def canonical(cells):
    forms = []
    for f in range(2):
        for r in range(4):
            transformed = []
            for x, y in cells:
                if f:
                    x = -x
                for _ in range(r):
                    x, y = y, -x
                transformed.append((x, y))
            forms.append(normalize(transformed))
    return min(forms)


def library(max_size=10):
    cache = ROOT / "shapes.json"
    if cache.exists():
        return json.loads(cache.read_text())
    shapes = {((0, 0),)}
    all_shapes = [list(next(iter(shapes)))]
    for size in range(2, max_size + 1):
        bigger = set()
        for shape in shapes:
            cells = set(shape)
            for x, y in shape:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    cell = x + dx, y + dy
                    if cell not in cells:
                        bigger.add(canonical(cells | {cell}))
        shapes = bigger
        all_shapes.extend(list(shape) for shape in shapes)
        print("enumerated", size, len(shapes), flush=True)
    cache.write_text(json.dumps(all_shapes))
    return all_shapes


def encode(pieces):
    result = [str(len(pieces))]
    for piece in pieces:
        result.append(str(len(piece)))
        result.extend(f"{x} {y}" for x, y in piece)
    return "\n".join(result) + "\n"


def generate(shapes, n, seed, mode="uniform"):
    rng = random.Random(seed)
    pieces = []
    for i in range(n):
        if mode == "uniform":
            shape = rng.choice(shapes)
        elif mode == "small":
            shape = rng.choice(shapes[:21])
        elif mode == "mono":
            shape = [(0, 0)]
        elif mode == "line":
            shape = [(x, 0) for x in range(10)]
        elif mode == "square":
            shape = [(x, y) for x in range(3) for y in range(3)]
        elif mode == "hole":
            shape = [(x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1)]
        elif mode == "single":
            shape = shapes[-seed % len(shapes)]
        else:
            raise ValueError(mode)
        f = rng.randrange(2)
        r = rng.randrange(4)
        tx, ty = rng.randrange(-100000, 100001), rng.randrange(-100000, 100001)
        shifted = []
        for x, y in shape:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            shifted.append((x + tx, y + ty))
        pieces.append(shifted)
    return pieces


def validate(pieces, output):
    values = list(map(int, output.split()))
    assert len(values) == 2 + 4 * len(pieces), (len(values), len(pieces))
    w, h = values[:2]
    assert w > 0 and h > 0
    used = set()
    for i, shape in enumerate(pieces):
        tx, ty, r, f = values[2 + 4 * i:6 + 4 * i]
        assert 0 <= r < 4 and 0 <= f < 2
        for x, y in shape:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            cell = x + tx, y + ty
            assert 0 <= cell[0] < w and 0 <= cell[1] < h, (i, cell, w, h)
            assert cell not in used, (i, cell)
            used.add(cell)
    return len(used) / (w * h), w, h


def run(binary, args, pieces):
    start = time.perf_counter()
    result = subprocess.run([str(binary), *map(str, args)], input=encode(pieces),
                            text=True, capture_output=True, timeout=15, check=True)
    elapsed = time.perf_counter() - start
    density, w, h = validate(pieces, result.stdout)
    return density, elapsed, w, h


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default=str(ROOT / "frontier"))
    parser.add_argument("--n", type=int, default=1000)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--mode", default="uniform")
    parser.add_argument("--widths", default="20,30,40,60")
    parser.add_argument("--windows", default="20,40,80")
    parser.add_argument("--trials", type=int, default=1)
    parser.add_argument("--lookahead", type=int, default=0)
    parser.add_argument("--weight", type=float, default=0.5)
    parser.add_argument("--auto", action="store_true")
    parser.add_argument("--write", default="")
    opts = parser.parse_args()
    shapes = library()
    pieces = generate(shapes, opts.n, opts.seed, opts.mode)
    if opts.write:
        Path(opts.write).write_text(encode(pieces))
    for width in map(int, opts.widths.split(",")):
        for window in map(int, opts.windows.split(",")):
            arguments = [] if opts.auto else [width, window, opts.trials, opts.lookahead, opts.weight]
            density, elapsed, w, h = run(opts.binary, arguments, pieces)
            print(f"n={opts.n} seed={opts.seed} mode={opts.mode} width={width} window={window} "
                  f"trials={opts.trials} lookahead={opts.lookahead} weight={opts.weight} "
                  f"density={density:.6f} time={elapsed:.4f} rect={w}x{h}", flush=True)


if __name__ == "__main__":
    main()
