import argparse
import random
import subprocess
import time

from validate import encode, transformed, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    args = parser.parse_args()
    cases = []
    line = [(i, 0) for i in range(10)]
    ring = [(x, y) for y in range(3) for x in range(3) if (x, y) != (1, 1)]
    cross = [(0, 2), (1, 2), (2, 2), (3, 2), (4, 2), (2, 0), (2, 1), (2, 3), (2, 4), (3, 4)]
    square = [(x, y) for y in range(3) for x in range(3)]
    for name, shape in [("unit", [(0, 0)]), ("line10", line), ("ring8", ring), ("cross10", cross), ("square9", square)]:
        for n in [100, 10000]:
            cases.append((name + "-" + str(n), [shape] * n))
    rng = random.Random(8931)
    for origin in [-2147483648, -2147483638, 2147483637, 2147483647, -10**15, 10**15]:
        shapes = []
        for i in range(100):
            shape = transformed(rng.choice([line, ring, cross, square]), rng.randrange(4), rng.randrange(2))
            shapes.append([(origin + x, -origin + y) for x, y in shape])
        cases.append((f"origin-{origin}", shapes))
    for name, shapes in cases:
        started = time.monotonic()
        run = subprocess.run([args.binary], input=encode(shapes), text=True, capture_output=True, timeout=10)
        elapsed = time.monotonic() - started
        assert run.returncode == 0, (name, "crash", run.returncode, run.stderr)
        density, width, height = validate(shapes, run.stdout)
        print(f"{name} density={density:.8f} rectangle={width}x{height} seconds={elapsed:.4f}", flush=True)


if __name__ == "__main__":
    main()
