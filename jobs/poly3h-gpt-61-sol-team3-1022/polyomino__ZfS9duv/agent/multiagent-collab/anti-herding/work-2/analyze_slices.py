import pickle
import random
import resource
import subprocess
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def main():
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(358761909)
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    for n in (100, 100, 150, 150, 300, 300, 500, 500, 1000, 3000):
        pieces = [rng.choice(shapes) for _ in range(n)]
        proc = subprocess.run([str(BASE / "slices_reference"), "0", "1.5"],
                              input=encode(pieces), text=True, stdout=subprocess.PIPE,
                              check=True, timeout=12)
        validate(pieces, proc.stdout)
        rows = proc.stdout.splitlines()
        long, width = map(int, rows[0].split())
        difference = [0] * (long + 2)
        filled = [0] * (long + 1)
        for piece, placement in zip(pieces, rows[1:]):
            tx, ty, r, f = map(int, placement.split())
            positions = []
            for x, y in piece:
                if f:
                    x = -x
                x, y = ((x, y), (y, -x), (-x, -y), (-y, x))[r]
                x += tx
                y += ty
                positions.append(x)
                filled[x + 1] += 1
            lo, hi = min(positions), max(positions)
            difference[lo + 1] += 1
            difference[hi + 1] -= 1
        cuts = []
        crossing = 0
        for x in range(long + 1):
            crossing += difference[x]
            if crossing == 0:
                cuts.append(x)
            if x:
                filled[x] += filled[x - 1]
        candidates = []
        for i, lo in enumerate(cuts):
            for hi in cuts[i + 1:]:
                if lo == 0 and hi == long:
                    continue
                area = filled[hi] - filled[lo]
                void = width * (hi - lo) - area
                if void >= width:
                    candidates.append((area, lo, hi, void))
        candidates.sort()
        print(f"n={n} board={width}x{long} void={width*long-filled[-1]} "
              f"cuts={len(cuts)} candidates={len(candidates)} "
              f"smallest={candidates[:4]}", flush=True)
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    print(f"cpu={after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime:.3f}")


if __name__ == "__main__":
    main()
