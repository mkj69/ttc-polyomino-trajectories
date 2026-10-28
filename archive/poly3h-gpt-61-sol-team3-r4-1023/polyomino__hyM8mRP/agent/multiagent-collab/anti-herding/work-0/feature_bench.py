import argparse
import importlib.util
import os
import random
import subprocess
import time
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "h", "/logs/agent/multiagent-collab/anti-herding/work-1/harness.py")
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
parser = argparse.ArgumentParser()
parser.add_argument("binary")
parser.add_argument("--feature", required=True)
parser.add_argument("--values", required=True)
parser.add_argument("--counts", default="100,200,333")
parser.add_argument("--seeds", type=int, default=5)
parser.add_argument("--seed", type=int, default=17847)
parser.add_argument("--widths", default="12,16,20")
args = parser.parse_args()
words = iter(map(int, Path("/logs/agent/multiagent-collab/anti-herding/work-1/catalog.in").read_text().split()))
catalog = []
for _ in range(next(words)):
    catalog.append(tuple((next(words), next(words)) for _ in range(next(words))))
cases = []
for n in map(int, args.counts.split(",")):
    for seed in range(args.seeds):
        rng = random.Random(args.seed + n * 100 + seed)
        shapes = [h.shifted(rng.choice(catalog), rng) for _ in range(n)]
        cases.append((n, shapes, h.encode(shapes)))
for value in args.values.split(","):
    env = os.environ.copy()
    env[args.feature] = value
    env["FIXED_REPAIR"] = "1"
    densities, runtimes, best = {}, [], {}
    for width in map(int, args.widths.split(",")):
        for case, (n, shapes, data) in enumerate(cases):
            started = time.monotonic()
            output = subprocess.run(
                [args.binary, str(width), "8", "32",
                 value if args.feature == "DIFFICULTY" else "7", "64", "48"],
                input=data, text=True, capture_output=True, timeout=15, env=env, check=True)
            runtimes.append(time.monotonic() - started)
            density = h.validate(shapes, output.stdout)[0]
            densities.setdefault(n, []).append(density)
            best[case] = max(best.get(case, 0), density)
    means = {n: round(sum(values) / len(values), 7) for n, values in densities.items()}
    all_values = [value for values in densities.values() for value in values]
    print(args.feature, value, "mean", round(sum(all_values) / len(all_values), 7),
          "best_width_mean", round(sum(best.values()) / len(best), 7),
          "by_n", means, "time", round(sum(runtimes), 3), flush=True)
