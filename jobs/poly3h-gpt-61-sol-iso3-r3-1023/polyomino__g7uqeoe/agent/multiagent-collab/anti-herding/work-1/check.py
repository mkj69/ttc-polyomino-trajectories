import argparse
import random
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def canonical(cells):
    forms = []
    for reflect in (False, True):
        for rotation in range(4):
            transformed = []
            for xx, yy in cells:
                x, y = -xx if reflect else xx, yy
                for _ in range(rotation):
                    x, y = y, -x
                transformed.append((x, y))
            mx = min(x for x, y in transformed)
            my = min(y for x, y in transformed)
            forms.append(tuple(sorted((x - mx, y - my) for x, y in transformed)))
    return min(forms)


def catalog():
    levels = [{((0, 0),)}]
    for size in range(2, 11):
        level = set()
        for shape in levels[-1]:
            occupied = set(shape)
            boundary = set()
            for x, y in shape:
                boundary.update(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
            for cell in boundary - occupied:
                level.add(canonical((*shape, cell)))
        levels.append(level)
    counts = list(map(len, levels))
    assert counts == [1, 1, 2, 5, 12, 35, 108, 369, 1285, 4655], counts
    return sorted(shape for level in levels for shape in level)


def encode(pieces):
    return str(len(pieces)) + "\n" + "".join(
        str(len(shape)) + "\n" + "".join(f"{x} {y}\n" for x, y in shape)
        for shape in pieces
    )


def check(pieces, output):
    values = list(map(int, output.split()))
    assert len(values) == 2 + 4 * len(pieces), len(values)
    width, height = values[:2]
    assert width > 0 and height > 0
    occupied = set()
    for i, piece in enumerate(pieces):
        tx, ty, r, f = values[2 + 4 * i : 6 + 4 * i]
        assert 0 <= r < 4 and 0 <= f < 2
        for xx, yy in piece:
            x, y = (-xx if f else xx), yy
            for _ in range(r):
                x, y = y, -x
            x += tx
            y += ty
            assert 0 <= x < width and 0 <= y < height, (i, x, y, width, height)
            assert (x, y) not in occupied, (i, x, y)
            occupied.add((x, y))
    assert len(occupied) == sum(map(len, pieces))
    return len(occupied) / (width * height)


def reference(pieces):
    width = 0
    height = 1
    placements = []
    for shape in pieces:
        minx = min(x for x, y in shape)
        miny = min(y for x, y in shape)
        maxx = max(x for x, y in shape)
        maxy = max(y for x, y in shape)
        placements.append((width - minx, -miny, 0, 0))
        width += maxx - minx + 1
        height = max(height, maxy - miny + 1)
    return f"{width} {height}\n" + "\n".join(" ".join(map(str, p)) for p in placements)


def run(pieces, extra=()):
    start = time.perf_counter()
    result = subprocess.run(
        [str(ROOT / "solution"), *extra],
        input=encode(pieces),
        text=True,
        capture_output=True,
        timeout=8,
        check=True,
    )
    elapsed = time.perf_counter() - start
    return check(pieces, result.stdout), elapsed, result.stderr.strip()


def random_shape(rng, size):
    cells = {(0, 0)}
    while len(cells) < size:
        x, y = rng.choice(tuple(cells))
        dx, dy = rng.choice(((0, 1), (0, -1), (1, 0), (-1, 0)))
        cells.add((x + dx, y + dy))
    ox, oy = rng.randint(-1000, 1000), rng.randint(-1000, 1000)
    return tuple((x + ox, y + oy) for x, y in cells)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--full", action="store_true")
    args = parser.parse_args()
    rng = random.Random(719)
    started = time.perf_counter()
    for trial in range(200):
        pieces = [random_shape(rng, rng.randrange(1, 11)) for _ in range(rng.randint(1, 120))]
        check(pieces, reference(pieces))
        run(pieces, ("1536", "8", "30", "0"))
    for shape in [
        ((0, 0),),
        tuple((x, 0) for x in range(10)),
        tuple((x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1)),
    ]:
        run([shape] * 100, ("1536", "8", "30", "0"))
    print(f"203 cross-validations valid; elapsed={time.perf_counter() - started:.3f}s")
    if args.full:
        shapes = catalog()
        print(f"catalog={len(shapes)}")
        for n in (100, 1000, 10000):
            for seed in range(3):
                sample_rng = random.Random(seed)
                pieces = [sample_rng.choice(shapes) for _ in range(n)]
                (ROOT / f"uniform-{n}-{seed}.in").write_text(encode(pieces))
                density, elapsed, info = run(pieces)
                control, control_time, _ = run(pieces, ("--no-pairs",))
                print(
                    f"n={n} seed={seed} density={density:.6f} no-pairs={control:.6f} "
                    f"time={elapsed:.3f}/{control_time:.3f} {info}"
                )


if __name__ == "__main__":
    main()
