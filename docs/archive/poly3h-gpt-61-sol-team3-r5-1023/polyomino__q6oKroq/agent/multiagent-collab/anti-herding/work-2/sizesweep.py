import argparse
import random
import statistics
import subprocess
import time

from check import encode, generate, validate

parser = argparse.ArgumentParser()
parser.add_argument("binary")
parser.add_argument("--n", type=int, default=100)
parser.add_argument("--cases", type=int, default=24)
parser.add_argument("--seed", type=int, default=4101)
parser.add_argument("--width", type=int, default=13)
parser.add_argument("--beam", type=int, default=128)
parser.add_argument("--weights", default="0,10,20,40,80")
args = parser.parse_args()
weights = list(map(int, args.weights.split(",")))
scores = {weight: [] for weight in weights}
times = {weight: [] for weight in weights}
wins = {weight: 0 for weight in weights}
losses = {weight: 0 for weight in weights}
for case in range(args.cases):
    data = generate(args.seed + case, args.n, "uniform")
    encoded = encode(data)
    order = list(weights)
    random.Random(args.seed + case).shuffle(order)
    densities = {}
    for weight in order:
        command = [args.binary] + (
            f"{args.width} 1200 10 -3 3 0 {args.beam} 12 1 10 1 0 25 0"
            f" 8 8 100 2000 2 100 0 100 0 {weight}"
        ).split()
        start = time.monotonic()
        result = subprocess.run(command, input=encoded, text=True, capture_output=True, timeout=8)
        times[weight].append(time.monotonic() - start)
        assert result.returncode == 0, result.stderr
        density, _ = validate(data, result.stdout)
        densities[weight] = density
        scores[weight].append(density)
    for weight in weights:
        wins[weight] += densities[weight] > densities[weights[0]] + 1e-12
        losses[weight] += densities[weight] < densities[weights[0]] - 1e-12
for weight in weights:
    print(f"weight={weight} n={args.n} cases={args.cases} "
          f"density={statistics.mean(scores[weight]):.6f} "
          f"time={statistics.mean(times[weight]):.4f} "
          f"wins={wins[weight]} losses={losses[weight]}")
