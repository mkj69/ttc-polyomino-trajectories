import argparse
import importlib.util
import random
import subprocess
import time
from pathlib import Path

spec = importlib.util.spec_from_file_location(
    "independent", "/logs/agent/multiagent-collab/anti-herding/work-1/harness.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)
parser = argparse.ArgumentParser()
parser.add_argument("binary")
parser.add_argument("--param", action="append", default=[])
parser.add_argument("--counts", default="100,333,1000,3333,10000")
parser.add_argument("--seeds", type=int, default=1)
args = parser.parse_args()
words = iter(map(int, Path("/logs/agent/multiagent-collab/anti-herding/work-1/catalog.in")
                 .read_text().split()))
catalog = []
for _ in range(next(words)):
    k = next(words)
    catalog.append(tuple((next(words), next(words)) for _ in range(k)))
cases = []
for n in map(int, args.counts.split(",")):
    for seed in range(args.seeds):
        rng = random.Random(17847 + n * 100 + seed)
        shapes = [harness.shifted(rng.choice(catalog), rng) for _ in range(n)]
        cases.append((n, shapes, harness.encode(shapes)))
for param in args.param or [""]:
    print(f"PARAM {param!r}", flush=True)
    densities, runtimes = [], []
    for n, shapes, data in cases:
        start = time.monotonic()
        result = subprocess.run([args.binary] + param.split(), input=data, text=True,
                                capture_output=True, check=True, timeout=30)
        runtime = time.monotonic() - start
        density, width, height = harness.validate(shapes, result.stdout)
        densities.append(density)
        runtimes.append(runtime)
        print(f"n={n} density={density:.6f} runtime={runtime:.4f} W={width} H={height}", flush=True)
    print(f"MEAN {sum(densities)/len(densities):.6f} "
          f"MAXTIME {max(runtimes):.4f} SUMTIME {sum(runtimes):.4f}", flush=True)
