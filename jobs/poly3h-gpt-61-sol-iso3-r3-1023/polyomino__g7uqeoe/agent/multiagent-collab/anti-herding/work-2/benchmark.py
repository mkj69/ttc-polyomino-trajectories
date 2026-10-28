import argparse
import pickle
import random
import subprocess
import time
from pathlib import Path

from validate import check, encode, generate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--compare")
    parser.add_argument("--sizes", default="100,1000,10000")
    parser.add_argument("--seeds", type=int, default=3)
    parser.add_argument("--generated", type=int, default=0)
    parser.add_argument("--catalog", action="store_true")
    parser.add_argument("--quiet", action="store_true")
    parser.add_argument("--base-seed", type=int, default=83214)
    parser.add_argument("extra", nargs="*")
    args = parser.parse_args()
    binaries = [args.binary] + ([args.compare] if args.compare else [])
    catalog = None
    if args.catalog:
        with open("/logs/agent/multiagent-collab/anti-herding/work-0/free-polyominoes.pkl", "rb") as handle:
            catalog = pickle.load(handle)
    sums = [0] * len(binaries)
    maxima = [0] * len(binaries)
    count = 0
    for n in map(int, args.sizes.split(",")):
        for seed in range(args.seeds):
            if catalog:
                rng = random.Random(args.base_seed + seed)
                pieces = [rng.choice(catalog) for _ in range(n)]
                text = encode(pieces)
            elif args.generated:
                pieces = generate(random.Random(args.base_seed + seed), n)
                text = encode(pieces)
            else:
                path = Path(f"/logs/agent/multiagent-collab/anti-herding/work-1/uniform-{n}-{seed}.in")
                text = path.read_text()
                numbers = list(map(int, text.split()))
                pieces, at = [], 1
                for _ in range(numbers[0]):
                    k = numbers[at]
                    at += 1
                    pieces.append(list(zip(numbers[at:at + 2 * k:2], numbers[at + 1:at + 2 * k:2])))
                    at += 2 * k
            count += 1
            results = []
            for j, binary in enumerate(binaries):
                options = args.extra if j == 0 else []
                start = time.monotonic()
                result = subprocess.run([binary, *options], input=text, text=True,
                                        capture_output=True, timeout=10, check=True)
                elapsed = time.monotonic() - start
                density = check(pieces, result.stdout)
                sums[j] += density
                maxima[j] = max(maxima[j], elapsed)
                results.append(f"{density:.6f}/{elapsed:.3f}s")
            if not args.quiet:
                print(f"n={n} seed={seed} " + " ".join(results), flush=True)
    print("mean=" + ",".join(f"{s / count:.6f}" for s in sums) +
          " max_time=" + ",".join(f"{t:.3f}" for t in maxima))


if __name__ == "__main__":
    main()
