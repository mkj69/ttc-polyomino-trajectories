import argparse
import random
import subprocess
import time

from check import generate, validate


EXE = "/logs/agent/multiagent-collab/anti-herding/work-2/main-snapshot"


def encode(pieces):
    return str(len(pieces)) + "\n" + "".join(
        str(len(p)) + "\n" + "".join(f"{x} {y}\n" for x, y in p)
        for p in pieces)


def reference(pieces):
    width, height = 0, 0
    rows = []
    for p in pieces:
        lx, ly = min(x for x, _ in p), min(y for _, y in p)
        w = max(x for x, _ in p) - lx + 1
        h = max(y for _, y in p) - ly + 1
        rows.append(f"{width-lx} {-ly} 0 0\n")
        width += w
        height = max(height, h)
    return f"{width} {height}\n" + "".join(rows)


def execute(pieces, args=()):
    t = time.monotonic()
    p = subprocess.run([EXE, *args], input=encode(pieces), text=True,
                       capture_output=True, check=True, timeout=5)
    elapsed = time.monotonic() - t
    density = validate(pieces, p.stdout)
    return density, elapsed


def small(count):
    for seed in range(count):
        pieces = generate(seed, 1 + seed % 14, "mixed")
        validate(pieces, reference(pieces))
        execute(pieces, ("16", "128", "1"))
    print(f"{count} independent small random cases: legal vs shelf reference",
          flush=True)


def large():
    cases = [
        ("monomino", generate(100, 10000, "monomino")),
        ("line", generate(100, 10000, "line")),
        ("hole", generate(100, 10000, "hole")),
        ("mixed", generate(109, 10000, "mixed")),
        ("hole-small", generate(100, 100, "hole")),
    ]
    pieces = []
    rng = random.Random(187)
    for i in range(10000):
        k = 1 + i % 10
        sx, sy = rng.choice([-10**12, 10**12]), rng.choice([-10**12, 10**12])
        pieces.append([(sx + x, sy) for x in range(k)])
    cases.append(("large-coordinates", pieces))
    for name, pieces in cases:
        d, t = execute(pieces)
        print(f"{name}: density={d:.6f} time={t:.4f}", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", default=EXE)
    parser.add_argument("--small", type=int, default=300)
    args = parser.parse_args()
    EXE = args.exe
    small(args.small)
    large()
