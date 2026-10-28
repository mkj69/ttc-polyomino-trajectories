import argparse
import random
import subprocess
import time

from check import encode, generate, validate

parser = argparse.ArgumentParser()
parser.add_argument("binary")
parser.add_argument("--cases", type=int, default=24)
parser.add_argument("--seed", type=int, default=3701)
parser.add_argument("--n", type=int, default=0)
parser.add_argument("--baseline")
parser.add_argument("--first", default="portfolio 256 0 1300 0 100 0 100 0")
parser.add_argument("--second", default="portfolio 256 0 1300 0 100 0 100 2")
args = parser.parse_args()
scores, times = [[], []], [[], []]
wins = losses = 0
for case in range(args.cases):
    seed = args.seed + case
    n = args.n or random.Random(seed).randint(100, 140)
    pieces = generate(seed, n, "uniform")
    data = encode(pieces)
    densities = [0, 0]
    for engine in ([0, 1] if case % 2 else [1, 0]):
        binary = args.baseline if engine == 0 and args.baseline else args.binary
        command = [binary] + [args.first, args.second][engine].split()
        start = time.monotonic()
        p = subprocess.run(command, input=data, text=True, capture_output=True, timeout=5)
        times[engine].append(time.monotonic() - start)
        assert p.returncode == 0, p.stderr
        density, _ = validate(pieces, p.stdout)
        scores[engine].append(density)
        densities[engine] = density
    wins += densities[1] > densities[0] + 1e-12
    losses += densities[1] < densities[0] - 1e-12
print(f"cases={args.cases} density={sum(scores[0])/args.cases:.6f}"
      f"->{sum(scores[1])/args.cases:.6f} wins={wins} losses={losses}"
      f" mean_time={sum(times[0])/args.cases:.3f}->{sum(times[1])/args.cases:.3f}"
      f" max_time={max(times[0]):.3f}->{max(times[1]):.3f}")
