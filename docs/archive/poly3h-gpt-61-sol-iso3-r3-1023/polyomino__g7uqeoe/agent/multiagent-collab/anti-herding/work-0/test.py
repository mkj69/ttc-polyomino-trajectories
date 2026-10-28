import argparse
import random
import subprocess
import time
import pickle
from pathlib import Path


def transform(cells, rotation, reflection):
    result = []
    for x, y in cells:
        if reflection:
            x = -x
        for _ in range(rotation):
            x, y = y, -x
        result.append((x, y))
    return result


def validate(shapes, output):
    lines = output.splitlines()
    assert len(lines) == len(shapes) + 1, (len(lines), len(shapes))
    width, height = map(int, lines[0].split())
    assert width > 0 and height > 0
    occupied = set()
    for index, (shape, line) in enumerate(zip(shapes, lines[1:])):
        tx, ty, rotation, reflection = map(int, line.split())
        assert 0 <= rotation < 4 and 0 <= reflection < 2
        for x, y in transform(shape, rotation, reflection):
            cell = x + tx, y + ty
            assert 0 <= cell[0] < width and 0 <= cell[1] < height, (index, cell, width, height)
            assert cell not in occupied, (index, cell)
            occupied.add(cell)
    return len(occupied) / (width * height)


def shelf_reference(shapes):
    x = 0
    height = 1
    placements = []
    for shape in shapes:
        min_x = min(a for a, b in shape)
        min_y = min(b for a, b in shape)
        max_x = max(a for a, b in shape)
        max_y = max(b for a, b in shape)
        placements.append(f"{x - min_x} {-min_y} 0 0")
        x += max_x - min_x + 1
        height = max(height, max_y - min_y + 1)
    return f"{max(1, x)} {height}\n" + "\n".join(placements) + "\n"


def generate(rng, n, size_mode):
    result = []
    for _ in range(n):
        k = rng.randrange(1, 11) if size_mode == "mixed" else int(size_mode)
        cells = {(0, 0)}
        while len(cells) < k:
            x, y = rng.choice(sorted(cells))
            dx, dy = rng.choice([(0, 1), (0, -1), (1, 0), (-1, 0)])
            cells.add((x + dx, y + dy))
        dx, dy = rng.randrange(-10**8, 10**8), rng.randrange(-10**8, 10**8)
        result.append([(x + dx, y + dy) for x, y in sorted(cells)])
    return result


def canonical(cells):
    variants = []
    for reflection in range(2):
        for rotation in range(4):
            candidate = transform(cells, rotation, reflection)
            min_x = min(x for x, y in candidate)
            min_y = min(y for x, y in candidate)
            variants.append(tuple(sorted((x - min_x, y - min_y) for x, y in candidate)))
    return min(variants)


def free_polyominoes(cache):
    if cache.exists():
        with cache.open("rb") as handle:
            return pickle.load(handle)
    current = {((0, 0),)}
    all_shapes = list(current)
    for size in range(2, 11):
        following = set()
        for shape in current:
            occupied = set(shape)
            for x, y in shape:
                for dx, dy in [(0, 1), (0, -1), (1, 0), (-1, 0)]:
                    if (x + dx, y + dy) not in occupied:
                        following.add(canonical(shape + ((x + dx, y + dy),)))
        current = following
        all_shapes.extend(sorted(current))
    with cache.open("wb") as handle:
        pickle.dump(all_shapes, handle)
    return all_shapes


def serialize(shapes):
    result = [str(len(shapes))]
    for shape in shapes:
        result.append(str(len(shape)))
        result.extend(f"{x} {y}" for x, y in shape)
    return "\n".join(result) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default=str(Path(__file__).with_name("solution")))
    parser.add_argument("--tests", type=int, default=100)
    parser.add_argument("--n", type=int, default=100)
    parser.add_argument("--size", default="mixed")
    parser.add_argument("--seed", type=int, default=419)
    parser.add_argument("--save")
    parser.add_argument("--uniform-free", action="store_true")
    parser.add_argument("--run-args", nargs="*", default=[])
    args = parser.parse_args()
    rng = random.Random(args.seed)
    catalog = free_polyominoes(Path(__file__).with_name("free-polyominoes.pkl")) if args.uniform_free else None
    densities, times = [], []
    for case in range(args.tests):
        shapes = [rng.choice(catalog) for _ in range(args.n)] if catalog else generate(rng, args.n, args.size)
        validate(shapes, shelf_reference(shapes))
        data = serialize(shapes)
        if args.save and case == 0:
            Path(args.save).write_text(data)
        begin = time.perf_counter()
        result = subprocess.run([args.binary] + args.run_args, input=data, text=True, capture_output=True, timeout=15)
        times.append(time.perf_counter() - begin)
        assert result.returncode == 0, result.stderr
        densities.append(validate(shapes, result.stdout))
    print(f"tests={args.tests} n={args.n} size={args.size} "
          f"density={sum(densities)/len(densities):.6f} "
          f"min_density={min(densities):.6f} "
          f"mean_time={sum(times)/len(times):.4f}s max_time={max(times):.4f}s")


if __name__ == "__main__":
    main()
