import argparse
import random
import subprocess
import time


def canonical(cells):
    variants = []
    for reflection in range(2):
        for rotation in range(4):
            transformed = []
            for x, y in cells:
                if reflection:
                    x = -x
                for _ in range(rotation):
                    x, y = y, -x
                transformed.append((x, y))
            minx = min(x for x, y in transformed)
            miny = min(y for x, y in transformed)
            variants.append(tuple(sorted((x - minx, y - miny) for x, y in transformed)))
    return min(variants)


def enumerate_free():
    current = {((0, 0),)}
    all_shapes = list(current)
    for size in range(2, 11):
        following = set()
        for shape in current:
            occupied = set(shape)
            neighbors = set()
            for x, y in shape:
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    if (x + dx, y + dy) not in occupied:
                        neighbors.add((x + dx, y + dy))
            for cell in neighbors:
                following.add(canonical(shape + (cell,)))
        current = following
        all_shapes.extend(sorted(current))
        print(f"free_size_{size}={len(current)}", flush=True)
    return all_shapes


def generate(rng, n, shapes=None):
    pieces = []
    for _ in range(n):
        if shapes:
            cells = rng.choice(shapes)
        else:
            k = rng.randint(1, 10)
            if rng.random() < 0.6:
                k = 10
            cells = {(0, 0)}
            while len(cells) < k:
                x, y = rng.choice(tuple(cells))
                dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
                cells.add((x + dx, y + dy))
        ox, oy = rng.randint(-100000, 100000), rng.randint(-100000, 100000)
        pieces.append([(x + ox, y + oy) for x, y in cells])
    return pieces


def encode(pieces):
    return str(len(pieces)) + "\n" + "".join(
        str(len(p)) + "\n" + "".join(f"{x} {y}\n" for x, y in p) for p in pieces
    )


def check(pieces, output):
    values = list(map(int, output.split()))
    assert len(values) == 2 + 4 * len(pieces), len(values)
    w, h = values[:2]
    assert w > 0 and h > 0
    occupied = set()
    for i, cells in enumerate(pieces):
        tx, ty, rotation, reflection = values[2 + 4 * i : 6 + 4 * i]
        assert 0 <= rotation <= 3 and reflection in (0, 1)
        for x, y in cells:
            if reflection:
                x = -x
            for _ in range(rotation):
                x, y = y, -x
            x, y = x + tx, y + ty
            assert 0 <= x < w and 0 <= y < h, (i, x, y, w, h)
            assert (x, y) not in occupied, (i, x, y)
            occupied.add((x, y))
    return len(occupied) / (w * h)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--cases", type=int, default=200)
    parser.add_argument("--large", type=int, default=0)
    parser.add_argument("--uniform", action="store_true")
    parser.add_argument("extra", nargs="*")
    args = parser.parse_args()
    rng = random.Random(149613)
    shapes = enumerate_free() if args.uniform else None
    densities, times = [], []
    for case in range(args.cases):
        n = args.large or rng.randint(1, 160)
        pieces = generate(rng, n, shapes)
        start = time.monotonic()
        result = subprocess.run([args.binary, *args.extra], input=encode(pieces), text=True,
                                capture_output=True, timeout=4, check=True)
        times.append(time.monotonic() - start)
        densities.append(check(pieces, result.stdout))
    print(f"cases={len(densities)} density_mean={sum(densities)/len(densities):.5f} "
          f"density_min={min(densities):.5f} seconds_max={max(times):.3f}")


if __name__ == "__main__":
    main()
