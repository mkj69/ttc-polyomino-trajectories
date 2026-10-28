import argparse
import itertools
import pickle
import random
import shlex
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).parent


def canonical(cells):
    options = []
    for reflected in range(2):
        for rotation in range(4):
            result = []
            for x, y in cells:
                if reflected:
                    x = -x
                for _ in range(rotation):
                    x, y = y, -x
                result.append((x, y))
            minx = min(x for x, y in result)
            miny = min(y for x, y in result)
            options.append(tuple(sorted((x - minx, y - miny) for x, y in result)))
    return min(options)


def all_shapes():
    cache = ROOT / "shapes.pickle"
    if cache.exists():
        return pickle.loads(cache.read_bytes())
    levels = [{((0, 0),)}]
    for size in range(2, 11):
        new = set()
        for cells in levels[-1]:
            occupied = set(cells)
            boundary = set()
            for x, y in cells:
                for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                    if (x + dx, y + dy) not in occupied:
                        boundary.add((x + dx, y + dy))
            for cell in boundary:
                new.add(canonical(cells + (cell,)))
        levels.append(new)
    shapes = sorted(itertools.chain.from_iterable(levels), key=lambda cells: (len(cells), cells))
    cache.write_bytes(pickle.dumps(shapes))
    print("shape counts", [len(level) for level in levels], flush=True)
    return shapes


def input_text(pieces):
    return str(len(pieces)) + "\n" + "".join(
        str(len(cells)) + "\n" + "".join(f"{x} {y}\n" for x, y in cells)
        for cells in pieces
    )


def validate(pieces, output):
    values = list(map(int, output.split()))
    assert len(values) == 2 + 4 * len(pieces), "wrong output length"
    width, height = values[:2]
    assert width > 0 and height > 0
    occupied = set()
    for index, cells in enumerate(pieces):
        tx, ty, rotation, reflected = values[2 + 4 * index:6 + 4 * index]
        assert 0 <= rotation <= 3 and 0 <= reflected <= 1
        for x, y in cells:
            if reflected:
                x = -x
            for _ in range(rotation):
                x, y = y, -x
            x += tx
            y += ty
            assert 0 <= x < width and 0 <= y < height, ("out of bounds", index, x, y)
            assert (x, y) not in occupied, ("overlap", index, x, y)
            occupied.add((x, y))
    return width * height


def reference(pieces):
    width, height = 0, 1
    placements = []
    for cells in pieces:
        minx = min(x for x, y in cells)
        miny = min(y for x, y in cells)
        w = max(x for x, y in cells) - minx + 1
        h = max(y for x, y in cells) - miny + 1
        placements.append((width - minx, -miny, 0, 0))
        width += w
        height = max(height, h)
    return f"{width} {height}\n" + "".join(" ".join(map(str, p)) + "\n" for p in placements)


def exact_minimum(pieces):
    total = sum(map(len, pieces))
    variants = []
    for cells in pieces:
        shapes = set()
        for f in range(2):
            for r in range(4):
                result = []
                for x, y in cells:
                    if f:
                        x = -x
                    for _ in range(r):
                        x, y = y, -x
                    result.append((x, y))
                minx = min(x for x, y in result)
                miny = min(y for x, y in result)
                shapes.add(tuple(sorted((x - minx, y - miny) for x, y in result)))
        variants.append(shapes)
    for area in range(total, total * 3 + 1):
        for height in range(1, int(area ** 0.5) + 1):
            if area % height:
                continue
            width = area // height
            masks = []
            for options in variants:
                current = []
                for cells in options:
                    w = max(x for x, y in cells) + 1
                    h = max(y for x, y in cells) + 1
                    for oy in range(height - h + 1):
                        for ox in range(width - w + 1):
                            current.append(sum(1 << ((y + oy) * width + x + ox) for x, y in cells))
                masks.append(current)
            masks.sort(key=len)

            def dfs(index, used):
                if index == len(masks):
                    return True
                return any(not (mask & used) and dfs(index + 1, mask | used)
                           for mask in masks[index])

            if dfs(0, 0):
                return area
    raise AssertionError("reference did not terminate")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default=str(ROOT / "solution"))
    parser.add_argument("--samples", type=int, default=200)
    parser.add_argument("--stress", action="store_true")
    parser.add_argument("--save", action="store_true")
    parser.add_argument("--flags", default="")
    args = parser.parse_args()
    random.seed(734927)
    shapes = all_shapes()
    command = [args.binary] + shlex.split(args.flags)
    ratios = []
    elapsed = []
    for case in range(args.samples):
        n = [1, 2, 3, 10, 100, 300][case % 6]
        pool = shapes[:8] if n <= 3 else shapes
        pieces = []
        for _ in range(n):
            dx, dy = random.randint(-10000, 10000), random.randint(-10000, 10000)
            pieces.append(tuple((x + dx, y + dy) for x, y in random.choice(pool)))
        data = input_text(pieces)
        start = time.perf_counter()
        process = subprocess.run(command, input=data, text=True, capture_output=True, check=True)
        elapsed.append(time.perf_counter() - start)
        area = validate(pieces, process.stdout)
        validate(pieces, reference(pieces))
        if n <= 3:
            assert area >= exact_minimum(pieces)
        ratios.append(sum(map(len, pieces)) / area)
    print(f"validated={args.samples} mean_density={sum(ratios) / len(ratios):.6f} max_seconds={max(elapsed):.4f}", flush=True)
    if args.stress:
        for case, n in enumerate((100, 1000, 10000, 10000, 10000)):
            if case == 3:
                pieces = [((0, 0), (1, 0), (2, 0), (0, 1), (2, 1), (0, 2), (1, 2), (2, 2))] * n
            elif case == 4:
                pieces = [tuple((x, 0) for x in range(10))] * n
            else:
                pieces = random.choices(shapes, k=n)
            data = input_text(pieces)
            start = time.perf_counter()
            process = subprocess.run(command, input=data, text=True, capture_output=True, check=True)
            seconds = time.perf_counter() - start
            area = validate(pieces, process.stdout)
            print(f"stress={case} n={n} density={sum(map(len, pieces)) / area:.6f} seconds={seconds:.4f}", flush=True)
            if args.save:
                (ROOT / f"case-{case}.in").write_text(data)


if __name__ == "__main__":
    main()
