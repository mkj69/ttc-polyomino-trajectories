import pickle
import random
import subprocess
import sys
import time

from check import encode, validate

with open("catalogue.pickle", "rb") as source:
    shapes = pickle.load(source)

binary = sys.argv[1]
binary_args = binary.split()
rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 3
sizes = list(map(int, sys.argv[3:])) if len(sys.argv) > 3 else [100, 150, 300, 1000, 3000, 10000]
for n in sizes:
    ratios = []
    runtimes = []
    for seed in range(rounds):
        rng = random.Random(9173 + seed)
        pieces = [rng.choice(shapes) for _ in range(n)]
        start = time.monotonic()
        result = subprocess.run(binary_args, input=encode(pieces), text=True, capture_output=True, check=True, timeout=5)
        if result.stderr:
            print(f"n={n} seed={seed} {result.stderr.strip()}", flush=True)
        runtimes.append(time.monotonic() - start)
        ratios.append(validate(pieces, result.stdout))
    print(f"n={n} density={sum(ratios)/len(ratios):.6f} min={min(ratios):.6f} "
          f"max_time={max(runtimes):.4f}s", flush=True)
