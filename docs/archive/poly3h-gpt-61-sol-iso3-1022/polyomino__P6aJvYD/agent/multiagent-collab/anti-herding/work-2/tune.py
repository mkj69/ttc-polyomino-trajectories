import itertools
import random
import subprocess
import sys
import time

from selftest import encode, validate
from uniformtest import parse


def main():
    rng = random.Random(178931)
    pool = parse(subprocess.run(["./generator", "0"], capture_output=True,
                               text=True, check=True).stdout)
    cases = []
    for n in [100, 100, 300, 1000, 3000]:
        pieces = [rng.choice(pool) for _ in range(n)]
        cases.append((pieces, encode(pieces)))
    best = (0, ())
    for samples, weight, shortlist in itertools.product(
            [48, 128], [0, 80, 160, 300, 600], [6, 16]):
        if weight == 0 and shortlist != 6:
            continue
        scores = []
        begin = time.monotonic()
        for pieces, data in cases:
            command = [sys.argv[1], "48", str(samples), "0", str(weight), str(shortlist)]
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
