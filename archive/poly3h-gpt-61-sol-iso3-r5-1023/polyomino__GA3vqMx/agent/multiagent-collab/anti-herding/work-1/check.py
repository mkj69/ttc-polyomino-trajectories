import argparse
import random
import subprocess
import time


def normalize(cells):
    x0 = min(x for x, y in cells)
    y0 = min(y for x, y in cells)
    return tuple(sorted((x - x0, y - y0) for x, y in cells))


def variants(cells):
    result = set()
    for reflect in range(2):
        for rotate in range(4):
            out = []
            for x, y in cells:
                x = -x if reflect else x
                for _ in range(rotate):
                    x, y = y, -x
                out.append((x, y))
            result.add(normalize(out))
    return result


def catalogue(maxsize):
    all_shapes = [((0, 0),)]
    level = {((0, 0),)}
    for size in range(2, maxsize + 1):
        next_level = set()
        for cells in level:
            occupied = set(cells)
            for x, y in cells:
                for dx, dy in [(0, 1), (1, 0), (0, -1), (-1, 0)]:
                    new = (x + dx, y + dy)
                    if new not in occupied:
                        next_level.add(min(variants(cells + (new,))))
        level = next_level
        all_shapes.extend(level)
    return all_shapes


def validate(shapes, text):
    lines = text.splitlines()
    assert len(lines) == len(shapes) + 1, ("line count", len(lines))
    width, height = map(int, lines[0].split())
    assert width > 0 and height > 0
    occupied = set()
    for index, (cells, line) in enumerate(zip(shapes, lines[1:])):
        tx, ty, rotate, reflect = map(int, line.split())
        assert 0 <= rotate < 4 and 0 <= reflect < 2
        for x, y in cells:
            x = -x if reflect else x
            for _ in range(rotate):
                x, y = y, -x
            x += tx
            y += ty
            assert 0 <= x < width and 0 <= y < height, ("bounds", index, x, y, width, height)
            assert (x, y) not in occupied, ("overlap", index, x, y)
            occupied.add((x, y))
    return len(occupied) / (width * height), width, height


def run(executable, shapes, timeout=10):
    data = str(len(shapes)) + "\n"
    for cells in shapes:
        data += str(len(cells)) + "\n" + "".join(f"{x} {y}\n" for x, y in cells)
    start = time.monotonic()
    result = subprocess.run([executable], input=data, text=True, capture_output=True, timeout=timeout)
    elapsed = time.monotonic() - start
    assert result.returncode == 0, result.stderr
    return validate(shapes, result.stdout), elapsed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executable")
    parser.add_argument("--cases", type=int, default=30)
    parser.add_argument("--maxn", type=int, default=150)
    parser.add_argument("--seed", type=int, default=1841)
    parser.add_argument("--enumerate", action="store_true")
    args = parser.parse_args()
    rng = random.Random(args.seed)
    if args.enumerate:
        shapes = catalogue(10)
        print("Catalogue:", len(shapes), flush=True)
    else:
        shapes = []
        for _ in range(1000):
            cells = {(0, 0)}
            for _ in range(rng.randint(0, 9)):
                choices = [(x + dx, y + dy) for x, y in cells
                           for dx, dy in [(0, 1), (1, 0), (0, -1), (-1, 0)]
                           if (x + dx, y + dy) not in cells]
                cells.add(rng.choice(choices))
            shapes.append(tuple(cells))
    density, elapsed = [], []
    for case in range(args.cases):
        count = args.maxn if args.cases < 5 else rng.randint(1, args.maxn)
        selected = []
        for _ in range(count):
            cells = rng.choice(shapes)
            dx, dy = rng.randint(-1000, 1000), rng.randint(-1000, 1000)
            selected.append(tuple((x + dx, y + dy) for x, y in cells))
        result, timing = run(args.executable, selected)
        density.append(result[0])
        elapsed.append(timing)
        if args.cases < 10:
            print(case, "density", round(result[0], 6), "rectangle", result[1:],
                  "seconds", round(timing, 3), flush=True)
    print("Cases", args.cases, "mean-density", sum(density) / len(density),
          "worst-time", max(elapsed), "total-seconds", sum(elapsed), flush=True)


if __name__ == "__main__":
    main()
