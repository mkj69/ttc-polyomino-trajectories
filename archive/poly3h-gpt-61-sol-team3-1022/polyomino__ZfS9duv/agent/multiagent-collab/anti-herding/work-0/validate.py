import argparse
import random
import subprocess
import time


def canonical(cells):
    forms = []
    for f in range(2):
        for r in range(4):
            points = []
            for x0, y0 in cells:
                x, y = (-x0 if f else x0), y0
                for _ in range(r):
                    x, y = y, -x
                points.append((x, y))
            xmin = min(x for x, y in points)
            ymin = min(y for x, y in points)
            forms.append(tuple(sorted((x - xmin, y - ymin) for x, y in points)))
    return min(forms)


def catalogue(limit):
    all_shapes = [((0, 0),)]
    level = set(all_shapes)
    for size in range(2, limit + 1):
        new = set()
        for cells in level:
            occupied = set(cells)
            for x, y in cells:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    q = (x + dx, y + dy)
                    if q not in occupied:
                        new.add(canonical(cells + (q,)))
        level = new
        all_shapes.extend(sorted(level))
        print("catalogue", size, len(level), flush=True)
    return all_shapes


def validate(shapes, output):
    values = list(map(int, output.split()))
    assert len(values) == 2 + 4 * len(shapes), (len(values), len(shapes))
    width, height = values[:2]
    assert width > 0 and height > 0
    occupied = set()
    for i, cells in enumerate(shapes):
        X, Y, r, f = values[2 + 4 * i:6 + 4 * i]
        assert 0 <= r < 4 and f in (0, 1)
        for x, y in cells:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            x += X
            y += Y
            assert 0 <= x < width and 0 <= y < height, (i, (x, y), (width, height))
            assert (x, y) not in occupied, (i, (x, y))
            occupied.add((x, y))
    return width, height, len(occupied) / (width * height)


def run(executable, shapes, timeout):
    text = str(len(shapes)) + "\n"
    for cells in shapes:
        text += str(len(cells)) + "\n"
        text += "".join(f"{x} {y}\n" for x, y in cells)
    start = time.perf_counter()
    result = subprocess.run([executable], input=text, text=True,
                            capture_output=True, timeout=timeout, check=True)
    elapsed = time.perf_counter() - start
    width, height, density = validate(shapes, result.stdout)
    return elapsed, width, height, density


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    parser.add_argument("--cases", type=int, default=200)
    parser.add_argument("--stress", type=int, default=10000)
    parser.add_argument("--catalogue", action="store_true")
    args = parser.parse_args()
    random.seed(7249121)
    worst_time = 0
    worst_density = 1
    for case in range(args.cases):
        shapes = []
        for _ in range(random.randint(1, 120)):
            cells = {(0, 0)}
            for size in range(1, random.randint(1, 10)):
                boundary = [(x + dx, y + dy) for x, y in cells
                            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
                            if (x + dx, y + dy) not in cells]
                cells.add(random.choice(boundary))
            X, Y = random.randint(-100000, 100000), random.randint(-100000, 100000)
            shapes.append([(x + X, y + Y) for x, y in cells])
        elapsed, width, height, density = run(args.executable, shapes, 30)
        worst_time = max(worst_time, elapsed)
        worst_density = min(worst_density, density)
    print("random cases", args.cases, "max seconds", round(worst_time, 4),
          "minimum density", round(worst_density, 5), flush=True)
    shapes = catalogue(10) if args.catalogue else [
        [(0, i) for i in range(10)],
        [(0, 0), (1, 0), (2, 0), (0, 1), (2, 1), (0, 2), (1, 2), (2, 2)],
        [(0, 0), (1, 0), (0, 1), (1, 1), (0, 2), (1, 2), (0, 3), (1, 3), (0, 4), (1, 4)],
        [(0, 0), (1, 0), (1, 1), (1, 2), (2, 2), (2, 3), (3, 3), (3, 4), (4, 4), (4, 5)],
    ]
    for n in (100, 1000, args.stress):
        chosen = [random.choice(shapes) for _ in range(n)]
        result = run(args.executable, chosen, 60)
        print("stress", n, "seconds/W/H/density", result, flush=True)


if __name__ == "__main__":
    main()
