import argparse
import pickle
import random
import subprocess
import sys
import time

from check import encode, validate

with open("catalogue.pickle", "rb") as source:
    shapes = pickle.load(source)

parser = argparse.ArgumentParser()
parser.add_argument("--rounds", type=int, default=4)
parser.add_argument("--seed", type=int, default=38197)
parser.add_argument("--breadth", type=int, default=96)
parser.add_argument("--policies", default="")
parser.add_argument("--binary", default="./feature-fixed")
parser.add_argument("--integrated", action="store_true")
parser.add_argument("sizes", nargs="*", type=int, default=[100, 150, 250])
args = parser.parse_args()
rounds = args.rounds
sizes = args.sizes
policies = [(0, 0)] + [(feature, weight) for feature in range(1, 7) for weight in (2, -2)]
if args.policies:
    policies = [tuple(map(int, policy.split(":"))) for policy in args.policies.split(",")]
results = {policy: [] for policy in policies}
for n in sizes:
    for seed in range(rounds):
        rng = random.Random(args.seed + seed)
        pieces = [rng.choice(shapes) for _ in range(n)]
        text = encode(pieces)
        for policy in policies:
            feature, weight = policy[:2]
            command = [args.binary, "-1", "3", "0", "0", "15000", "34", "10", "1", "0",
                       str(args.breadth), "24", "10", "3", "10000", "0", "0", "8", "3", "0", "0", "64",
                       "150", "1", "128", "0", str(feature), str(weight), "1"]
            if len(policy) > 2:
                command.append(str(policy[2]))
            if args.integrated:
                command[2] = "6" if n >= 200 else "3"
                command[3] = "10000"
                command[4] = "80"
                command[9] = "100"
                command[10] = "0"
                command[14] = "1600"
                command[25] = "1"
                command[28] = "5"
            start = time.monotonic()
            proc = subprocess.run(command, input=text, text=True, capture_output=True, check=True, timeout=12)
            density = validate(pieces, proc.stdout)
            results[policy].append(density)
            print(f"n={n} seed={seed} policy={policy} "
                  f"density={density:.7f} time={time.monotonic()-start:.4f}", flush=True)
    baseline = results[policies[0]][-rounds:]
    for policy in policies:
        values = results[policy][-rounds:]
        gain = sum(a - b for a, b in zip(values, baseline)) / rounds
        print(f"SUMMARY n={n} policy={policy} density={sum(values)/rounds:.7f} gain={gain:.7f}", flush=True)
