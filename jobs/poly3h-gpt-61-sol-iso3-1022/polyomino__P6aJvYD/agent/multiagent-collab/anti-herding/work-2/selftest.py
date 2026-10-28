import random
import subprocess
import sys
import time


def generate(rng, n, max_k=10):
    pieces = []
    for _ in range(n):
        k = rng.randint(1, max_k)
        cells = {(0, 0)}
        while len(cells) < k:
            x, y = rng.choice(tuple(cells))
            dx, dy = rng.choice(((0, 1), (0, -1), (1, 0), (-1, 0)))
            cells.add((x + dx, y + dy))
        ox, oy = rng.randrange(-1000, 1001), rng.randrange(-1000, 1001)
        pieces.append([(x + ox, y + oy) for x, y in cells])
    return pieces


def encode(pieces):
    lines = [str(len(pieces))]
    for cells in pieces:
        lines.append(str(len(cells)))
        lines.extend(f"{x} {y}" for x, y in cells)
    return "\n".join(lines) + "\n"


def validate(pieces, output):
    tokens = [int(t) for t in output.split()]
    assert len(tokens) == 2 + 4 * len(pieces), len(tokens)
    w, h = tokens[:2]
    assert w > 0 and h > 0
    occupied = set()
    for i, cells in enumerate(pieces):
        tx, ty, r, f = tokens[2 + 4 * i:6 + 4 * i]
        assert 0 <= r <= 3 and 0 <= f <= 1
        for x, y in cells:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            x, y = x + tx, y + ty
            assert 0 <= x < w and 0 <= y < h, (i, (x, y), (w, h))
            assert (x, y) not in occupied, (i, (x, y))
            occupied.add((x, y))
    return len(occupied) / (w * h)


def main():
    binary = sys.argv[1]
    command = [binary] + sys.argv[3:]
    rng = random.Random(768193)
    rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    densities = []
    started = time.monotonic()
    for case in range(rounds):
        n = rng.randrange(1, 201)
        pieces = generate(rng, n)
        result = subprocess.run(command, input=encode(pieces), text=True,
                                capture_output=True, timeout=3, check=True)
        densities.append(validate(pieces, result.stdout))
    for n in [100, 1000, 10000]:
        pieces = generate(rng, n)
        begin = time.monotonic()
        result = subprocess.run(command, input=encode(pieces), text=True,
                                capture_output=True, timeout=3, check=True)
        density = validate(pieces, result.stdout)
        print(f"stress n={n} density={density:.5f} seconds={time.monotonic()-begin:.3f}")
    print(f"random={rounds} mean_density={sum(densities)/len(densities):.5f} "
          f"seconds={time.monotonic()-started:.3f}")


if __name__ == "__main__":
    main()
