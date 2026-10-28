import random
import subprocess
import sys
import time


def canonical(cells):
    result = []
    for f in range(2):
        for r in range(4):
            transformed = []
            for x, y in cells:
                if f:
                    x = -x
                for _ in range(r):
                    x, y = y, -x
                transformed.append((x, y))
            minx = min(x for x, y in transformed)
            miny = min(y for x, y in transformed)
            result.append(tuple(sorted((x - minx, y - miny) for x, y in transformed)))
    return min(result)


def generate(n, seed, mode):
    random.seed(seed)
    pieces = []
    for i in range(n):
        if mode == "bars":
            shape = [(j, 0) for j in range(10)]
        elif mode == "cross":
            shape = [(0, 1), (1, 0), (1, 1), (2, 1), (1, 2)]
        elif mode == "rings":
            shape = [(x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1)]
        elif mode == "single":
            shape = [(0, 0)]
        else:
            k = random.randint(1, 10) if mode == "mixed" else 10
            cells = {(0, 0)}
            while len(cells) < k:
                x, y = random.choice(tuple(cells))
                dx, dy = random.choice([(1, 0), (-1, 0), (0, 1), (0, -1)])
                cells.add((x + dx, y + dy))
            shape = list(cells)
        r = random.randrange(4)
        f = random.randrange(2)
        sx, sy = random.randint(-100000, 100000), random.randint(-100000, 100000)
        transformed = []
        for x, y in shape:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            transformed.append((x + sx, y + sy))
        pieces.append(transformed)
    return pieces


def input_text(pieces):
    lines = [str(len(pieces))]
    for piece in pieces:
        lines.append(str(len(piece)))
        lines.extend(f"{x} {y}" for x, y in piece)
    return "\n".join(lines) + "\n"


def validate(pieces, text):
    values = list(map(int, text.split()))
    assert len(values) == 2 + 4 * len(pieces), len(values)
    w, h = values[:2]
    assert w > 0 and h > 0
    occupied = set()
    for i, piece in enumerate(pieces):
        sx, sy, r, f = values[2 + 4 * i:6 + 4 * i]
        assert 0 <= r < 4 and 0 <= f < 2
        for x, y in piece:
            if f:
                x = -x
            for _ in range(r):
                x, y = y, -x
            x += sx
            y += sy
            assert 0 <= x < w and 0 <= y < h, (i, x, y, w, h)
            assert (x, y) not in occupied, (i, x, y)
            occupied.add((x, y))
    return len(occupied) / (w * h), w, h


if __name__ == "__main__":
    binary = sys.argv[1]
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 30
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 100
    mode = sys.argv[4] if len(sys.argv) > 4 else "mixed"
    params = sys.argv[5:]
    ratios = []
    elapsed = []
    for seed in range(count):
        pieces = generate(n, seed, mode)
        start = time.monotonic()
        output = subprocess.check_output([binary] + params, input=input_text(pieces).encode(), timeout=10)
        elapsed.append(time.monotonic() - start)
        ratio, w, h = validate(pieces, output)
        ratios.append(ratio)
        if count < 10:
            print(f"seed={seed} n={n} mode={mode} density={ratio:.6f} rect={w}x{h} time={elapsed[-1]:.4f}")
    print(f"cases={count} n={n} mode={mode} mean_density={sum(ratios)/count:.6f} min_density={min(ratios):.6f} max_time={max(elapsed):.4f}")
