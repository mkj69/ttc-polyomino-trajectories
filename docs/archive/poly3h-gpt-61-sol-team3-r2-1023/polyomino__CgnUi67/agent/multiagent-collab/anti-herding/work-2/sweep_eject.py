import argparse
import subprocess
import sys
import time

sys.path.insert(0, "/logs/agent/multiagent-collab/anti-herding/work-0")
from check import generate, validate


def serialize(pieces):
    return str(len(pieces)) + "\n" + "".join(
        str(len(p)) + "\n" + "".join(f"{x} {y}\n" for x, y in p)
        for p in pieces)


parser = argparse.ArgumentParser()
parser.add_argument("--exe", default="/logs/agent/multiagent-collab/anti-herding/"
                                    "work-2/eject")
parser.add_argument("--initial", default="150,350,650,1000")
parser.add_argument("--variants", default="0")
parser.add_argument("--features", default="0")
parser.add_argument("--seed", type=int, default=61)
parser.add_argument("--cases", type=int, default=8)
parser.add_argument("--n", type=int, default=100)
args = parser.parse_args()
cases = [generate(args.n, args.seed + k, "uniform") for k in range(args.cases)]
texts = [serialize(p) for p in cases]
for initial in map(int, args.initial.split(",")):
    for config in ((int(v), int(f)) for v in args.variants.split(",")
                   for f in args.features.split(",")):
        variant, features = config
        densities, seconds, shrinks = [], [], []
        for k, (pieces, text) in enumerate(zip(cases, texts)):
            start = time.monotonic()
            r = subprocess.run([args.exe, str(initial), "1700", str(variant),
                                str(features)],
                               input=text, text=True, capture_output=True,
                               check=True, timeout=10)
            elapsed = time.monotonic() - start
            d, w, h = validate(pieces, r.stdout)
            shrunk = r.stderr.count("SHRUNK")
            densities.append(d)
            seconds.append(elapsed)
            shrinks.append(shrunk)
            print(f"initial={initial} variant={variant} features={features} n={args.n} "
                  f"seed={args.seed+k} d={d:.7f} t={elapsed:.4f} "
                  f"shrinks={shrunk}", flush=True)
        print(f"SUMMARY initial={initial} variant={variant} features={features} n={args.n} "
              f"mean={sum(densities)/len(densities):.7f} "
              f"shrinks={sum(shrinks)} maxsec={max(seconds):.4f}", flush=True)
