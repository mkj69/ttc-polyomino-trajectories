import argparse
import random
import shlex
import subprocess
import time


def generate(rng, n, mode):
    pieces = []
    for i in range(n):
        if mode == "uniform":
            cells = rng.choice(catalogue)
            ox, oy = rng.randint(-100, 100), rng.randint(-100, 100)
            pieces.append([(x + ox, y + oy) for x, y in cells])
            continue
        k = 10 if mode == "large" else rng.randint(1, 10)
        if mode == "tiny":
            k = rng.randint(1, 4)
        if mode == "line":
            cells = [(j, 0) for j in range(k)]
        else:
            cells = {(0, 0)}
            while len(cells) < k:
                x, y = rng.choice(tuple(cells))
                dx, dy = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
                cells.add((x + dx, y + dy))
            cells = list(cells)
        ox, oy = rng.randint(-100, 100), rng.randint(-100, 100)
        pieces.append([(x + ox, y + oy) for x, y in cells])
    return pieces


def encode(pieces):
    lines = [str(len(pieces))]
    for p in pieces:
        lines.append(str(len(p)))
        lines.extend(f"{x} {y}" for x, y in p)
    return "\n".join(lines) + "\n"


def validate(pieces, output):
    values = [int(t) for t in output.split()]
    assert len(values) == 2 + 4 * len(pieces), len(values)
    w, h = values[:2]
    assert w > 0 and h > 0
    occupied = set()
    for i, cells in enumerate(pieces):
        tx, ty, r, f = values[2 + 4 * i:6 + 4 * i]
        assert 0 <= r <= 3 and 0 <= f <= 1
        for x, y in cells:
            if f:
                x = -x
            if r == 1:
                x, y = y, -x
            elif r == 2:
                x, y = -x, -y
            elif r == 3:
                x, y = -y, x
            point = (x + tx, y + ty)
            assert 0 <= point[0] < w and 0 <= point[1] < h, (i, point, w, h)
            assert point not in occupied, (i, point)
            occupied.add(point)
    return len(occupied) / (w * h)


def reference(pieces):
    x = 0
    h = 0
    lines = []
    for cells in pieces:
        lx, ly = min(a for a, b in cells), min(b for a, b in cells)
        rx, ry = max(a for a, b in cells), max(b for a, b in cells)
        lines.append(f"{x-lx} {-ly} 0 0")
        x += rx - lx + 1
        h = max(h, ry - ly + 1)
    return f"{x} {h}\n" + "\n".join(lines) + "\n"


def main():
    global catalogue
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", default="/logs/agent/multiagent-collab/anti-herding/work-0/solution")
    parser.add_argument("--count", type=int, default=300)
    parser.add_argument("--stress", action="store_true")
    parser.add_argument("--uniform", action="store_true")
    parser.add_argument("--n", type=int, default=0)
    parser.add_argument("--opts", default="")
    parser.add_argument("--compare-exe", default="")
    parser.add_argument("--diagnostics", action="store_true")
    args = parser.parse_args()
    if args.uniform:
        data = iter(map(int, open("/logs/agent/multiagent-collab/anti-herding/work-0/catalogue.txt").read().split()))
        catalogue = []
        for _ in range(next(data)):
            catalogue.append([(next(data), next(data)) for _ in range(next(data))])
    rng = random.Random(71632)
    total = 0
    start = time.monotonic()
    for index in range(args.count):
        n = args.n or (10000 if args.stress else rng.randint(1, 120))
        mode = "uniform" if args.uniform else ("large", "mixed", "tiny", "line")[index % 4]
        pieces = generate(rng, n, mode)
        validate(pieces, reference(pieces))
        before = time.monotonic()
        result = subprocess.run([args.exe] + shlex.split(args.opts), input=encode(pieces), text=True,
                                capture_output=True, timeout=10, check=True)
        ratio = validate(pieces, result.stdout)
        if args.diagnostics:
            print(result.stderr.strip())
        if args.compare_exe:
            other = subprocess.run([args.compare_exe] + shlex.split(args.opts), input=encode(pieces),
                                   text=True, capture_output=True, timeout=10, check=True)
            assert result.stdout == other.stdout
        elapsed = time.monotonic() - before
        total += ratio
        if args.stress:
            print(f"mode={mode} n={n} density={ratio:.6f} seconds={elapsed:.3f}")
    print(f"valid={args.count} average_density={total/args.count:.6f} seconds={time.monotonic()-start:.3f}")


if __name__ == "__main__":
    main()
