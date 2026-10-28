import argparse
import pickle
import random
import subprocess
import time

from validate import check, encode, generate


def run(binary, pieces, output):
    result = subprocess.run([binary], input=encode(pieces) + output, text=True,
                            capture_output=True, timeout=10, check=True)
    before = check(pieces, output)
    after = check(pieces, result.stdout)
    assert after >= before, (before, after)
    return before, after


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True)
    parser.add_argument("--baseline")
    parser.add_argument("--cases", type=int, default=200)
    parser.add_argument("--sizes", default="100,220,1000,10000")
    args = parser.parse_args()
    rng = random.Random(19569323)
    ring = [(x, y) for y in range(3) for x in range(3) if (x, y) != (1, 1)]
    pieces = [ring, [(0, 0)]]
    before, after = run(args.binary, pieces, "3 8\n0 5 0 0\n1 6 0 0\n")
    assert abs(after - 1.0) < 1e-12, (before, after)
    started = time.monotonic()
    for case in range(args.cases):
        n = rng.randint(1, 160)
        pieces = generate(rng, n)
        width = 10 * n
        lines = [f"{width} 40"]
        for i, shape in enumerate(pieces):
            reflection, rotation = rng.randrange(2), rng.randrange(4)
            transformed = []
            for x, y in shape:
                if reflection:
                    x = -x
                for _ in range(rotation):
                    x, y = y, -x
                transformed.append((x, y))
            tx = i * 10 - min(x for x, y in transformed)
            ty = rng.randrange(20) - min(y for x, y in transformed)
            lines.append(f"{tx} {ty} {rotation} {reflection}")
        run(args.binary, pieces, "\n".join(lines) + "\n")
    print(f"arbitrary={args.cases} valid seconds={time.monotonic() - started:.3f}", flush=True)
    if not args.baseline:
        return
    with open("/logs/agent/multiagent-collab/anti-herding/work-0/free-polyominoes.pkl", "rb") as handle:
        catalog = pickle.load(handle)
    for n in map(int, args.sizes.split(",")):
        old_sum = new_sum = 0.0
        wins = 0
        durations = []
        for case in range(8):
            pieces = [rng.choice(catalog) for _ in range(n)]
            output = subprocess.run([args.baseline], input=encode(pieces), text=True,
                                    capture_output=True, timeout=10, check=True).stdout
            started = time.monotonic()
            before, after = run(args.binary, pieces, output)
            durations.append(time.monotonic() - started)
            old_sum += before
            new_sum += after
            wins += after > before
            print(f"n={n} case={case} before={before:.8f} after={after:.8f}", flush=True)
        print(f"n={n} wins={wins}/8 mean={old_sum/8:.8f}->{new_sum/8:.8f} "
              f"maxSeconds={max(durations):.3f}", flush=True)


if __name__ == "__main__":
    main()
