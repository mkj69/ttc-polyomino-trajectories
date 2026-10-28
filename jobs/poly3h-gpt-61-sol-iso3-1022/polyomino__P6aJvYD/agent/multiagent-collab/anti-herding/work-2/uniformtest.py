import random
import subprocess
import sys
import time

from selftest import encode, validate


def parse(data):
    words = iter(map(int, data.split()))
    return [[(next(words), next(words)) for _ in range(next(words))]
            for _ in range(next(words))]


def main():
    binary = sys.argv[1]
    command = [binary] + sys.argv[2:]
    generated = subprocess.run(["./generator", "0"], capture_output=True,
                               text=True, check=True)
    pool = parse(generated.stdout)
    print(f"free_shapes={len(pool)} counts={generated.stderr.strip()}")
    rng = random.Random(178931)
    scores = []
    for n in [100, 100, 100, 300, 1000, 3000, 10000]:
        pieces = [rng.choice(pool) for _ in range(n)]
        begin = time.monotonic()
        result = subprocess.run(command, input=encode(pieces), text=True,
                                capture_output=True, timeout=3, check=True)
        density = validate(pieces, result.stdout)
        scores.append(density)
        print(f"n={n} density={density:.6f} seconds={time.monotonic()-begin:.3f} "
              f"stats={result.stderr.strip()}")
    print(f"mean_density={sum(scores)/len(scores):.6f}")


if __name__ == "__main__":
    main()
