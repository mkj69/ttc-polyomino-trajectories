import random
import subprocess
import sys
import time


def transform(shape, r, f):
    out = []
    for x, y in shape:
        if f:
            x = -x
        for _ in range(r):
            x, y = y, -x
        out.append((x, y))
    return out


def normalize(shape):
    mx = min(x for x, y in shape)
    my = min(y for x, y in shape)
    return tuple(sorted((x - mx, y - my) for x, y in shape))


def canonical(shape):
    return min(normalize(transform(shape, r, f)) for r in range(4) for f in range(2))


def validate(shapes, output):
    lines = output.splitlines()
    assert len(lines) == len(shapes) + 1, (len(lines), len(shapes))
    w, h = map(int, lines[0].split())
    assert w > 0 and h > 0
    occupied = set()
    for shape, line in zip(shapes, lines[1:]):
        xx, yy, r, f = map(int, line.split())
        assert 0 <= r < 4 and 0 <= f < 2
        for x, y in transform(shape, r, f):
            cell = x + xx, y + yy
            assert 0 <= cell[0] < w and 0 <= cell[1] < h, cell
            assert cell not in occupied, cell
            occupied.add(cell)
    return len(occupied) / (w * h), w, h


def random_shape(rng, k):
    shape = {(0, 0)}
    while len(shape) < k:
        x, y = rng.choice(tuple(shape))
        dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
        shape.add((x + dx, y + dy))
    shape = transform(shape, rng.randrange(4), rng.randrange(2))
    ox, oy = rng.randrange(-100, 100), rng.randrange(-100, 100)
    return [(x + ox, y + oy) for x, y in shape]


def run(binary, shapes):
    data = str(len(shapes)) + "\n"
    for shape in shapes:
        data += str(len(shape)) + "\n"
        data += "".join(f"{x} {y}\n" for x, y in shape)
    start = time.monotonic()
    p = subprocess.run([binary], input=data, text=True, capture_output=True, timeout=4)
    assert p.returncode == 0, p.stderr
    elapsed = time.monotonic() - start
    result = validate(shapes, p.stdout)
    return elapsed, result


if __name__ == "__main__":
    rng = random.Random(19)
    binary = sys.argv[1]
    if len(sys.argv) > 2:
        data = list(map(int, open(sys.argv[2]).read().split()))
        it = iter(data)
        shapes = []
        for _ in range(next(it)):
            shapes.append([(next(it), next(it)) for j in range(next(it))])
        print("uniform file", len(shapes), run(binary, shapes), flush=True)
        sys.exit(0)
    for n in [1, 3, 100, 100, 1000, 10000]:
        shapes = [random_shape(rng, rng.randint(1, 10) if n < 1000 else 10) for i in range(n)]
        print(n, run(binary, shapes), flush=True)
    for k in range(1, 11):
        shapes = [[(j - 23, 14) for j in range(k)] for i in range(100)]
        print("lines", k, run(binary, shapes), flush=True)
    for i in range(100):
        n = rng.randrange(1, 50)
        shapes = [random_shape(rng, rng.randint(1, 10)) for j in range(n)]
        run(binary, shapes)
    print("100 random small tests valid")
