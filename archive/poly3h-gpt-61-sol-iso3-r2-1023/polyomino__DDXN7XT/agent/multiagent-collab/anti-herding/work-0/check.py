import itertools
import math
import os
import pickle
import random
import subprocess
import sys
import time

ROOT = "/logs/agent/multiagent-collab/anti-herding/work-0"
EXECUTABLE = sys.argv[2] if len(sys.argv) > 2 else "./solution"
random.seed(1792968802)


def transform(cells, r, f):
    result = []
    for x, y in cells:
        if f:
            x = -x
        for _ in range(r):
            x, y = y, -x
        result.append((x, y))
    return result


def orientations(cells):
    result = set()
    for r in range(4):
        for f in range(2):
            p = transform(cells, r, f)
            mx = min(x for x, y in p)
            my = min(y for x, y in p)
            result.add(tuple(sorted((x - mx, y - my) for x, y in p)))
    return result


def canonical(cells):
    return min(orientations(cells))


def random_shape(k):
    cells = {(0, 0)}
    while len(cells) < k:
        x, y = random.choice(tuple(cells))
        dx, dy = random.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
        cells.add((x + dx, y + dy))
    ox, oy = random.randint(-10000, 10000), random.randint(-10000, 10000)
    return [(x + ox, y + oy) for x, y in cells]


def run(shapes, executable=None):
    executable = executable or EXECUTABLE
    text = [str(len(shapes))]
    for shape in shapes:
        text.append(str(len(shape)))
        text += [f"{x} {y}" for x, y in shape]
    start = time.monotonic()
    result = subprocess.run(
        [executable], input="\n".join(text) + "\n", text=True,
        capture_output=True, check=True, timeout=10, cwd=ROOT,
    )
    elapsed = time.monotonic() - start
    tokens = [int(v) for v in result.stdout.split()]
    assert len(tokens) == 2 + len(shapes) * 4, (len(tokens), len(shapes))
    w, h = tokens[:2]
    assert w >= 0 and h >= 0
    occupied = set()
    for i, shape in enumerate(shapes):
        x, y, r, f = tokens[2 + 4 * i:6 + 4 * i]
        assert 0 <= r < 4 and 0 <= f < 2
        for dx, dy in transform(shape, r, f):
            p = (x + dx, y + dy)
            assert 0 <= p[0] < w and 0 <= p[1] < h, (i, p, w, h)
            assert p not in occupied, (i, p)
            occupied.add(p)
    return w * h, elapsed


def exact_area(shapes):
    variants = [list(orientations(s)) for s in shapes]
    order = sorted(range(len(shapes)), key=lambda i: -len(shapes[i]))
    total = sum(map(len, shapes))
    upper = sum(max(x for x, y in shapes[i]) - min(x for x, y in shapes[i]) + 1
                for i in order) * max(
        max(y for x, y in s) - min(y for x, y in s) + 1 for s in shapes
    )
    for area in range(total, upper + 1):
        for w in range(1, area + 1):
            if area % w:
                continue
            h = area // w
            placements = []
            possible = True
            for i in order:
                masks = []
                for shape in variants[i]:
                    sw = max(x for x, y in shape) + 1
                    sh = max(y for x, y in shape) + 1
                    for y in range(h - sh + 1):
                        for x in range(w - sw + 1):
                            masks.append(sum(1 << ((y + dy) * w + x + dx)
                                             for dx, dy in shape))
                if not masks:
                    possible = False
                    break
                placements.append(masks)
            if not possible:
                continue

            def search(i, occupied):
                if i == len(placements):
                    return True
                return any(not occupied & mask and search(i + 1, occupied | mask)
                           for mask in placements[i])

            if search(0, 0):
                return area
    raise AssertionError("No packing")


def all_free_shapes(max_size=10):
    cache = ROOT + f"/free-{max_size}.pickle"
    if os.path.exists(cache):
        with open(cache, "rb") as stream:
            return pickle.load(stream)
    shapes = {((0, 0),)}
    result = list(shapes)
    for k in range(2, max_size + 1):
        next_shapes = set()
        for shape in shapes:
            occupied = set(shape)
            for x, y in shape:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    p = (x + dx, y + dy)
                    if p not in occupied:
                        next_shapes.add(canonical((*shape, p)))
        shapes = next_shapes
        result += sorted(shapes)
        print("enumerated", k, len(shapes), flush=True)
    with open(cache, "wb") as stream:
        pickle.dump(result, stream)
    return result


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "quick"
    if mode == "uniform":
        pool = all_free_shapes()
        for n in (100, 300, 1000, 3000, 10000):
            for _ in range(2):
                shapes = [random.choice(pool) for _ in range(n)]
                area, elapsed = run(shapes)
                print("uniform", n, "density", sum(map(len, shapes)) / area,
                      "time", elapsed, flush=True)
        return
    for case in range(250):
        n = random.randint(1, 60)
        shapes = [random_shape(random.randint(1, 10)) for _ in range(n)]
        area, elapsed = run(shapes)
        assert area >= sum(map(len, shapes))
        if case < 25:
            small = [random_shape(random.randint(1, 3)) for _ in range(random.randint(1, 4))]
            heuristic, _ = run(small)
            optimum = exact_area(small)
            assert heuristic >= optimum
    print("250 random cases and 25 exact references valid", flush=True)
    for k in range(1, 11):
        shapes = [random_shape(k) for _ in range(100)]
        area, elapsed = run(shapes)
        print("size", k, "density", 100 * k / area, "time", elapsed, flush=True)
    for n in (100, 1000, 10000):
        shapes = [random_shape(random.choices(range(1, 11), weights=(1, 1, 2, 5, 12, 35, 108, 369, 1285, 4655))[0])
                  for _ in range(n)]
        area, elapsed = run(shapes)
        print("stress", n, "density", sum(map(len, shapes)) / area, "time", elapsed, flush=True)


if __name__ == "__main__":
    main()
