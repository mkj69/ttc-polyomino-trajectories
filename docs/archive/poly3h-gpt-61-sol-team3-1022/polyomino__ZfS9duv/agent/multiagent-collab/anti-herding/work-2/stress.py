import pickle
import random
import resource
import subprocess
import sys
import time
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def run(binary, name, pieces, arguments):
    old_cpu = resource.getrusage(resource.RUSAGE_CHILDREN)
    begin = time.monotonic()
    result = subprocess.run([binary, *arguments], input=encode(pieces),
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, timeout=25)
    assert result.returncode == 0, (name, result.stderr)
    wall = time.monotonic() - begin
    usage = resource.getrusage(resource.RUSAGE_CHILDREN)
    cpu = usage.ru_utime + usage.ru_stime - old_cpu.ru_utime - old_cpu.ru_stime
    area = validate(pieces, result.stdout)
    w, h = map(int, result.stdout.splitlines()[0].split())
    assert h <= w, (w, h)
    density = sum(map(len, pieces)) / area
    print(f"{name}: n={len(pieces)} density={density:.8f} "
          f"cpu={cpu:.6f} wall={wall:.6f}", flush=True)


def main():
    binary = sys.argv[1]
    arguments = sys.argv[2:]
    rng = random.Random(6721883)
    ring = [(x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1)]
    patterns = {
        "mono": [(0, 0)],
        "line10": [(0, y) for y in range(10)],
        "ring8": ring,
        "slab10": [(x, y) for x in range(2) for y in range(5)],
        "snake10": [(x, y) for x, y in [
            (0, 0), (1, 0), (1, 1), (1, 2), (2, 2),
            (2, 3), (3, 3), (3, 4), (4, 4), (4, 5)]],
    }
    for name, shape in patterns.items():
        shifted = [(x + 10**12, y - 10**12) for x, y in shape]
        run(binary, name + "-single-extreme", [shifted], arguments)
        run(binary, name + "-100", [shifted] * 100, arguments)
        run(binary, name + "-10000", [shifted] * 10000, arguments)
    catalogue = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    run(binary, "all6473types", catalogue, arguments)
    for n in (100, 1000, 10000):
        chosen = []
        for _ in range(n):
            original = rng.choice(catalogue)
            ox, oy = rng.randint(-10**12, 10**12), rng.randint(-10**12, 10**12)
            chosen.append([(x + ox, y + oy) for x, y in original])
        run(binary, "uniform" + str(n), chosen, arguments)


if __name__ == "__main__":
    main()
