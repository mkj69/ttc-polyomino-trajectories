import argparse
import random
import subprocess
import time
from pathlib import Path


def normalize(shape):
    mx = min(x for x, y in shape)
    my = min(y for x, y in shape)
    return tuple(sorted((x - mx, y - my) for x, y in shape))


def transform(shape, r, f):
    result = []
    for x, y in shape:
        if f:
            x = -x
        for _ in range(r):
            x, y = y, -x
        result.append((x, y))
    return result


def canonical(shape):
    return min(normalize(transform(shape, r, f)) for r in range(4) for f in range(2))


def free_catalog(max_size=10):
    level = {((0, 0),)}
    result = []
    for size in range(1, max_size + 1):
        result.extend(sorted(level))
        if size == max_size:
            break
        following = set()
        for shape in level:
            occupied = set(shape)
            for x, y in shape:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    cell = (x + dx, y + dy)
                    if cell not in occupied:
                        following.add(canonical((*shape, cell)))
        level = following
    return result


def random_shape(rng, k):
    occupied = {(0, 0)}
    while len(occupied) < k:
        x, y = rng.choice(tuple(occupied))
        dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
        occupied.add((x + dx, y + dy))
    return normalize(occupied)


def encode(shapes):
    lines = [str(len(shapes))]
    for shape in shapes:
        lines.append(str(len(shape)))
        lines.extend(f"{x} {y}" for x, y in shape)
    return "\n".join(lines) + "\n"


def validate(shapes, output):
    values = [int(word) for word in output.split()]
    if len(values) != 2 + 4 * len(shapes):
        raise AssertionError(f"wrong output length: {len(values)}")
    width, height = values[:2]
    if width <= 0 or height <= 0:
        raise AssertionError("nonpositive dimensions")
    occupied = set()
    for i, shape in enumerate(shapes):
        tx, ty, r, f = values[2 + 4 * i:6 + 4 * i]
        if not (0 <= r < 4 and 0 <= f <= 1):
            raise AssertionError(f"invalid transform for {i}")
        for x, y in transform(shape, r, f):
            x += tx
            y += ty
            if not (0 <= x < width and 0 <= y < height):
                raise AssertionError(f"outside rectangle for piece {i}: {x}, {y}")
            if (x, y) in occupied:
                raise AssertionError(f"overlap for piece {i}: {x}, {y}")
            occupied.add((x, y))
    return len(occupied) / (width * height), width, height


def run(binary, shapes, limit=5):
    started = time.monotonic()
    result = subprocess.run([binary], input=encode(shapes), text=True,
                            capture_output=True, timeout=limit, check=True)
    elapsed = time.monotonic() - started
    density, width, height = validate(shapes, result.stdout)
    return density, elapsed, width, height


def shifted(shape, rng):
    result = transform(shape, rng.randrange(4), rng.randrange(2))
    dx, dy = rng.randint(-10000, 10000), rng.randint(-10000, 10000)
    return [(x + dx, y + dy) for x, y in result]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", nargs="?")
    parser.add_argument("--trials", type=int, default=100)
    parser.add_argument("--catalog")
    parser.add_argument("--generate-catalog")
    parser.add_argument("--large", action="store_true")
    args = parser.parse_args()
    rng = random.Random(20261026)
    if args.generate_catalog:
        started = time.monotonic()
        catalog = free_catalog()
        Path(args.generate_catalog).write_text(encode(catalog))
        print(f"catalog: {len(catalog)} shapes, {time.monotonic() - started:.3f}s")
        return
    catalog = None
    if args.catalog:
        words = iter(map(int, Path(args.catalog).read_text().split()))
        count = next(words)
        catalog = []
        for _ in range(count):
            k = next(words)
            catalog.append(tuple((next(words), next(words)) for _ in range(k)))
    cases = [
        [((0, 0),)],
        [tuple((i, 0) for i in range(10))] * 100,
        [tuple((x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1))] * 100,
        [((0, 0),)] * 100,
        [((0, 0), (1, 0), (1, 1))] * 100,
    ]
    for _ in range(args.trials):
        count = rng.randrange(1, 300) if not args.large else rng.choice((100, 1000, 10000))
        shapes = [rng.choice(catalog) if catalog else random_shape(rng, rng.randrange(1, 11))
                  for _ in range(count)]
        cases.append([shifted(shape, rng) for shape in shapes])
    densities = []
    times = []
    for i, shapes in enumerate(cases):
        density, elapsed, width, height = run(args.binary, shapes)
        densities.append(density)
        times.append(elapsed)
        if args.large:
            print(f"case {i}: n={len(shapes)} density={density:.6f} "
                  f"time={elapsed:.4f} W={width} H={height}", flush=True)
    print(f"valid {len(cases)}; mean density={sum(densities) / len(densities):.6f}; "
          f"worst={min(densities):.6f}; maximum runtime={max(times):.4f}s")


if __name__ == "__main__":
    main()
