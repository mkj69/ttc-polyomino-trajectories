import argparse
import collections
import pickle
import random
import resource
import subprocess
import sys

sys.path.insert(0, "/logs/agent/multiagent-collab/anti-herding/work-1")
from check import encode, generate, validate

parser = argparse.ArgumentParser()
parser.add_argument("binaries", nargs="+")
parser.add_argument("--cases", type=int, default=30)
parser.add_argument("--seed", type=int, default=995000)
parser.add_argument("--sizes", type=int, nargs="+", default=[100, 200, 350, 500, 700])
parser.add_argument("--widths", type=int, nargs="+", default=[12, 19, 29])
args = parser.parse_args()
with open("/logs/agent/multiagent-collab/anti-herding/work-2/catalogue.pickle", "rb") as f:
    shapes = pickle.load(f)
times = collections.defaultdict(float)
scores = collections.defaultdict(list)
wins = collections.Counter()
for seed in range(args.cases):
    pieces = generate(random.Random(args.seed + seed), args.sizes[seed % len(args.sizes)], shapes)
    data = encode(pieces)
    results = []
    for binary in args.binaries:
        best = 0
        for width in args.widths:
            before = resource.getrusage(resource.RUSAGE_CHILDREN)
            result = subprocess.run([binary, str(width)], input=data, text=True,
                                    capture_output=True, check=True)
            after = resource.getrusage(resource.RUSAGE_CHILDREN)
            times[binary] += after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
            best = max(best, validate(pieces, result.stdout))
        scores[binary].append(best)
        results.append(best)
    winners = [args.binaries[i] for i, score in enumerate(results) if score == max(results)]
    wins["tie" if len(winners) > 1 else winners[0]] += 1
print("All valid; CPU", dict(times))
print("Mean density", {name: sum(values) / len(values) for name, values in scores.items()})
print("Wins", dict(wins))
