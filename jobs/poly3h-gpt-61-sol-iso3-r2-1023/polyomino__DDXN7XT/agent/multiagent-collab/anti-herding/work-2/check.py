import random
import subprocess
import sys
import time

catalog = []


def generate(n, seed, mode="uniform"):
    rng = random.Random(seed)
    pieces = []
    for _ in range(n):
        if mode == "free":
            shape = rng.choice(catalog)
        else:
            k = rng.randint(1, 10) if mode == "uniform" else 10
            shape = {(0, 0)}
            while len(shape) < k:
                x, y = rng.choice(tuple(shape))
                dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
                shape.add((x + dx, y + dy))
        xoffset, yoffset = rng.randint(-100, 100), rng.randint(-100, 100)
        pieces.append([(x + xoffset, y + yoffset) for x, y in shape])
    return pieces


def encode(pieces):
    return str(len(pieces)) + "\n" + "".join(
        str(len(piece)) + "\n" + "".join(f"{x} {y}\n" for x, y in piece)
        for piece in pieces
    )


def validate(pieces, output):
    numbers = list(map(int, output.split()))
    assert len(numbers) == 2 + 4 * len(pieces), len(numbers)
    width, height = numbers[:2]
    assert width > 0 and height > 0
    occupied = set()
    for i, piece in enumerate(pieces):
        tx, ty, rotation, flip = numbers[2 + 4 * i: 6 + 4 * i]
        assert 0 <= rotation < 4 and flip in (0, 1)
        for x, y in piece:
            if flip:
                x = -x
            for _ in range(rotation):
                x, y = y, -x
            x, y = x + tx, y + ty
            assert 0 <= x < width and 0 <= y < height, (i, x, y, width, height)
            assert (x, y) not in occupied, (i, x, y)
            occupied.add((x, y))
    return len(occupied) / (width * height)


def run(binary, n, seed, mode):
    pieces = generate(n, seed, mode)
    start = time.perf_counter()
    result = subprocess.run([binary], input=encode(pieces), text=True,
                            capture_output=True, timeout=4, check=True)
    elapsed = time.perf_counter() - start
    density = validate(pieces, result.stdout)
    return density, elapsed


if __name__ == "__main__":
    binary = sys.argv[1]
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    if len(sys.argv) > 3:
        numbers = iter(map(int, subprocess.check_output([sys.argv[3]], text=True).split()))
        for _ in range(next(numbers)):
            catalog.append([(next(numbers), next(numbers)) for _ in range(next(numbers))])
    samples = []
    for seed in range(count):
        n = 1 + seed % 30
        samples.append(run(binary, n, seed, "uniform"))
    print("small", count, "mean density", sum(d for d, _ in samples) / count,
          "max time", max(t for _, t in samples))
    for n in (100, 1000, 10000):
        samples = [run(binary, n, seed, "free" if catalog else "ten") for seed in range(3)]
        print(n, "density", [round(d, 5) for d, _ in samples],
              "seconds", [round(t, 4) for _, t in samples])
