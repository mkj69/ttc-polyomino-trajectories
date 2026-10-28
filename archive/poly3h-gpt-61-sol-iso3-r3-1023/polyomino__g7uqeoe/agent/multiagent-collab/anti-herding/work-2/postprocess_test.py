import argparse
import pickle
import random
import subprocess
import time
from pathlib import Path

from validate import check, encode


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--sizes", default="100,220,1000")
    parser.add_argument("--samples", type=int, default=8)
    parser.add_argument("--seed", type=int, default=73891234)
    parser.add_argument("--stats", action="store_true")
    args = parser.parse_args()
    cache = Path("/logs/agent/multiagent-collab/anti-herding/work-2/packing-cache")
    cache.mkdir(exist_ok=True)
    with open("/logs/agent/multiagent-collab/anti-herding/work-0/free-polyominoes.pkl", "rb") as handle:
        catalog = pickle.load(handle)
    total_wins = total = 0
    for n in map(int, args.sizes.split(",")):
        old_sum = new_sum = 0.0
        wins = 0
        durations = []
        for case in range(args.samples):
            rng = random.Random(args.seed + n * 239 + case)
            pieces = [rng.choice(catalog) for _ in range(n)]
            text = encode(pieces)
            output_path = cache / f"capped-{args.seed}-{n}-{case}.out"
            if output_path.exists():
                output = output_path.read_text()
            else:
                output = subprocess.run([args.baseline], input=text, text=True,
                                        capture_output=True, timeout=10, check=True).stdout
                check(pieces, output)
                output_path.write_text(output)
            before = check(pieces, output)
            started = time.monotonic()
            result = subprocess.run([args.binary], input=text + output, text=True,
                                    capture_output=True, timeout=10, check=True)
            durations.append(time.monotonic() - started)
            after = check(pieces, result.stdout)
            assert after >= before, (n, case, before, after)
            old_sum += before
            new_sum += after
            wins += after > before
            total_wins += after > before
            total += 1
            if args.stats:
                print(result.stderr, end="", flush=True)
            print(f"n={n} case={case} before={before:.8f} after={after:.8f} "
                  f"seconds={durations[-1]:.3f}", flush=True)
        print(f"n={n} wins={wins}/{args.samples} mean={old_sum/args.samples:.8f}"
              f"->{new_sum/args.samples:.8f} maxSeconds={max(durations):.3f}", flush=True)
    print(f"totalWins={total_wins}/{total}", flush=True)


if __name__ == "__main__":
    main()
