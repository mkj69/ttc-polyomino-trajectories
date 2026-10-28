import pickle
import random
import subprocess
import sys
import time

from check import encode, validate

with open("catalogue.pickle", "rb") as source:
    shapes = pickle.load(source)

rounds = int(sys.argv[1])
sizes = list(map(int, sys.argv[2:]))
commands = [
    ["./base-fixed", "-1", "3", "0", "0", "15000", "34", "10", "1", "0",
     "256", "24", "10", "3", "12000", "0", "0", "8", "3", "0", "0", "64", "150", "1", "128", "0"],
    ["./table-fixed", "-1", "3", "0", "0", "15000", "34", "10", "1", "0",
     "256", "24", "10", "3", "12000", "0", "0", "8", "3", "0", "0", "64", "150", "1", "128", "0"],
]
for n in sizes:
    elapsed = [[], []]
    ratios = [[], []]
    for seed in range(rounds):
        rng = random.Random(9173 + seed)
        pieces = [rng.choice(shapes) for _ in range(n)]
        text = encode(pieces)
        for candidate, command in enumerate(commands):
            start = time.monotonic()
            proc = subprocess.run(command, input=text, text=True, capture_output=True, check=True, timeout=20)
            elapsed[candidate].append(time.monotonic() - start)
            ratios[candidate].append(validate(pieces, proc.stdout))
            print(f"n={n} seed={seed} candidate={candidate} density={ratios[candidate][-1]:.8f} "
                  f"time={elapsed[candidate][-1]:.4f} {proc.stderr.strip()}", flush=True)
    print(f"n={n} speedup={sum(elapsed[0])/sum(elapsed[1]):.3f} "
          f"equal_areas={ratios[0] == ratios[1]}", flush=True)
