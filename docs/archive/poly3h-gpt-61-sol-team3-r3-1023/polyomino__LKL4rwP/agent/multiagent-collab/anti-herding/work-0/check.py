import argparse
import random
import subprocess
import time


def transform(cells, rotation, reflection):
    result = []
    for x, y in cells:
        if reflection:
            x = -x
        for _ in range(rotation):
            x, y = y, -x
        result.append((x, y))
    return result


def canonical(cells):
    forms = []
    for reflection in range(2):
        for rotation in range(4):
            shape = transform(cells, rotation, reflection)
            minx = min(x for x, _ in shape)
            miny = min(y for _, y in shape)
            forms.append(tuple(sorted((x - minx, y - miny) for x, y in shape)))
    return min(forms)


def catalogue(max_size):
    shapes = {((0, 0),)}
    all_shapes = list(shapes)
    for size in range(2, max_size + 1):
        next_shapes = set()
        for shape in shapes:
            occupied = set(shape)
            boundary = {(x + dx, y + dy) for x, y in shape
                        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))}
            for cell in boundary - occupied:
                next_shapes.add(canonical(list(shape) + [cell]))
        shapes = next_shapes
        all_shapes.extend(sorted(shapes))
    return all_shapes


def random_shape(rng, size):
    cells = {(0, 0)}
    while len(cells) < size:
        x, y = rng.choice(tuple(sorted(cells)))
        dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
        cells.add((x + dx, y + dy))
    return tuple(cells)


def validate(pieces, output):
    values = list(map(int, output.split()))
    assert len(values) == 2 + 4 * len(pieces), "wrong output length"
    width, height = values[:2]
    assert width > 0 and height > 0
    occupied = set()
    for i, cells in enumerate(pieces):
        x, y, rotation, reflection = values[2 + 4 * i:6 + 4 * i]
        assert 0 <= rotation <= 3 and 0 <= reflection <= 1
        for dx, dy in transform(cells, rotation, reflection):
            cell = x + dx, y + dy
            assert 0 <= cell[0] < width and 0 <= cell[1] < height, (i, cell, width, height)
            assert cell not in occupied, (i, cell, "overlap")
            occupied.add(cell)
    return len(occupied) / (width * height)


def reference(pieces):
    placements = []
    width = 0
    height = 0
    for cells in pieces:
        minx = min(x for x, _ in cells)
        miny = min(y for _, y in cells)
        maxx = max(x for x, _ in cells)
        maxy = max(y for _, y in cells)
        placements.extend((width - minx, -miny, 0, 0))
        width += maxx - minx + 1
        height = max(height, maxy - miny + 1)
    return " ".join(map(str, [width, height] + placements))


def run(executable, pieces, args=()):
    lines = [str(len(pieces))]
    for cells in pieces:
        lines.append(str(len(cells)))
        lines.extend(f"{x} {y}" for x, y in cells)
    start = time.monotonic()
    process = subprocess.run([executable, *args], input="\n".join(lines) + "\n", text=True,
                             capture_output=True, check=True, timeout=5)
    elapsed = time.monotonic() - start
    return validate(pieces, process.stdout), elapsed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    parser.add_argument("--cases", type=int, default=300)
    parser.add_argument("--catalogue", action="store_true")
    parser.add_argument("--stress", type=int, default=10000)
    parser.add_argument("--single", action="store_true")
    args = parser.parse_args()
    rng = random.Random(347852)
    shapes = catalogue(10) if args.catalogue else None
    if shapes:
        print("catalogue", len(shapes), flush=True)
    densities = []
    elapsed = 0.0
    for case in range(args.cases):
        n = rng.randint(1, 120)
        pieces = []
        for _ in range(n):
            shape = rng.choice(shapes) if shapes else random_shape(rng, rng.randint(1, 10))
            shape = transform(shape, rng.randrange(4), rng.randrange(2))
            tx, ty = rng.randint(-10000, 10000), rng.randint(-10000, 10000)
            pieces.append([(x + tx, y + ty) for x, y in shape])
        validate(pieces, reference(pieces))
        density, duration = run(args.executable, pieces, ("20",) if args.single else ())
        densities.append(density)
        elapsed += duration
    print("small", args.cases, "density", sum(densities) / len(densities),
          "min", min(densities), "time", elapsed, flush=True)
    if args.stress:
        pieces = [rng.choice(shapes) if shapes else random_shape(rng, 10)
                  for _ in range(args.stress)]
        density, duration = run(args.executable, pieces, ("63",) if args.single else ())
        print("stress", len(pieces), "density", density, "time", duration, flush=True)


if __name__ == "__main__":
    main()
