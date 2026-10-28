import argparse
import pickle
import random
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, "/logs/agent/multiagent-collab/anti-herding/work-0")
from validate import catalogue, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary")
    parser.add_argument("--n", type=int, default=10000)
    parser.add_argument("--seed", type=int, default=541891)
    parser.add_argument("--count", type=int, default=1)
    parser.add_argument("args", nargs="*")
    args, rest = parser.parse_known_args()
    cache = BASE / "catalogue.pickle"
    if cache.exists():
        shapes = pickle.loads(cache.read_bytes())
    else:
        shapes = catalogue(10)
        cache.write_bytes(pickle.dumps(shapes))
    rng = random.Random(args.seed)
    for iteration in range(args.count):
        chosen = [rng.choice(shapes) for _ in range(args.n)]
        text = str(len(chosen)) + "\n"
        for piece in chosen:
            ox, oy = rng.randrange(-1000, 1001), rng.randrange(-1000, 1001)
            text += str(len(piece)) + "\n"
            text += "".join(f"{x+ox} {y+oy}\n" for x, y in piece)
        (BASE / f"catalogue-{args.n}-{iteration}.in").write_text(text)
        numbers = iter(map(int, text.split()))
        n = next(numbers)
        chosen = [[(next(numbers), next(numbers)) for _ in range(next(numbers))]
                  for _ in range(n)]
        start = time.monotonic()
        result = subprocess.run([args.binary, *args.args, *rest], input=text,
                                stdout=subprocess.PIPE, text=True, check=True,
                                timeout=30)
        elapsed = time.monotonic() - start
        width, height, density = validate(chosen, result.stdout)
        print(f"n={args.n} seed={args.seed} iteration={iteration} "
              f"area={width*height} density={density:.8f} time={elapsed:.5f}",
              flush=True)


if __name__ == "__main__":
    main()
