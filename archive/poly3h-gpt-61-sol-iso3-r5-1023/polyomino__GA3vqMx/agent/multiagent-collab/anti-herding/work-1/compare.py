import argparse
import random
import shlex
from check import catalogue, run


parser = argparse.ArgumentParser()
parser.add_argument("executables", nargs="+")
parser.add_argument("--sizes", default="100,300,1000,10000")
parser.add_argument("--seeds", type=int, default=2)
args = parser.parse_args()
shapes = catalogue(10)
for n in map(int, args.sizes.split(",")):
    for seed in range(args.seeds):
        rng = random.Random(1841 + seed)
        selected = [rng.choice(shapes) for _ in range(n)]
        for executable in args.executables:
            result, elapsed = run(executable, selected, timeout=30)
            print("n", n, "seed", seed, "exe", executable, "density", round(result[0], 7),
                  "size", result[1:], "time", round(elapsed, 4), flush=True)
