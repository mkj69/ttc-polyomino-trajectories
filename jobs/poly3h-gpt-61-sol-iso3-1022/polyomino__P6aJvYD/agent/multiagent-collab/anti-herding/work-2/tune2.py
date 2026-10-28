import itertools
import math
import random
import subprocess
import sys
import time

from selftest import encode, validate
from uniformtest import parse


def main():
    rng = random.Random(767893)
    pool = parse(subprocess.run(["./generator", "0"], capture_output=True,
                               text=True, check=True).stdout)
    cases = []
    for n in [100, 150, 300, 1000, 3000]:
        pieces = [rng.choice(pool) for _ in range(n)]
        width = min(64, max(10, int(math.sqrt(sum(map(len, pieces))) * 0.65)))
        cases.append((pieces, encode(pieces), width))
    best = (0, ())
    for samples, weight, shortlist in itertools.product(
            [128, 256], [80, 160, 300, 600], [0, 2, 6]):
        scores = []
        begin = time.monotonic()
        for pieces, data, width in cases:
            command = [sys.argv[1], str(width), str(samples), "0", str(weight), str(shortlist)]
            result = subprocess.run(command, input=data, capture_output=True,
                                    text=True, check=True, timeout=5)
            scores.append(validate(pieces, result.stdout))
        mean = sum(scores) / len(scores)
        key = samples, weight, shortlist
        best = max(best, (mean, key))
        print(f"{key} mean={mean:.6f} seconds={time.monotonic()-begin:.3f} "
              f"densities={[round(s, 5) for s in scores]}", flush=True)
    print(f"best={best}")


if __name__ == "__main__":
    main()
