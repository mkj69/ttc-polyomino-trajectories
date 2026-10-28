import argparse
import random
import subprocess
import sys
import time

sys.path.insert(0, "/logs/agent/multiagent-collab/anti-herding/work-0")
from check import generate, validate

parser = argparse.ArgumentParser()
parser.add_argument("--exe", required=True)
parser.add_argument("--reference")
parser.add_argument("--values", default="0,1")
parser.add_argument("--n", default="100,200")
parser.add_argument("--seed", type=int, default=901)
parser.add_argument("--cases", type=int, default=6)
parser.add_argument("--runs", default="100000")
parser.add_argument("--initial", default="650")
parser.add_argument("--attempt", default="250")
parser.add_argument("--prefix-extra", default="")
args = parser.parse_args()
values = list(map(int, args.values.split(",")))
stats = {v: [] for v in values}
rng = random.Random(args.seed)
for n in map(int, args.n.split(",")):
    for k in range(args.cases):
        pieces = generate(n, args.seed + k, "uniform")
        text = str(n) + "\n" + "".join(
            str(len(p)) + "\n" + "".join(f"{x} {y}\n" for x, y in p)
            for p in pieces
        )
        order = values.copy()
        rng.shuffle(order)
        for value in order:
            start = time.monotonic()
            extra = args.prefix_extra.split(",") if args.prefix_extra else []
            executable = args.reference if value == 0 and args.reference else args.exe
            run = subprocess.run(
                [executable, "0", "256", args.runs, "-1", args.initial,
                 "0", "1", args.attempt, "0", "20"] + extra + [str(value)],
                input=text, text=True, capture_output=True, check=True, timeout=10,
            )
            density, w, h = validate(pieces, run.stdout)
            elapsed = time.monotonic() - start
            stats[value].append((density, elapsed))
            print(f"n={n} seed={args.seed+k} value={value} "
                  f"density={density:.9f} area={w*h} sec={elapsed:.4f}", flush=True)
for value, results in stats.items():
    print(f"SUMMARY value={value} count={len(results)} "
          f"mean={sum(d for d, _ in results)/len(results):.9f} "
          f"maxsec={max(t for _, t in results):.4f}", flush=True)
