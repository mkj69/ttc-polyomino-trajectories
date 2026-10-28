import argparse
import pickle
import random
import subprocess
import time
from pathlib import Path
from test import free_polyominoes, serialize, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base", default=str(Path(__file__).with_name("patch-control")))
    parser.add_argument("--optimizer", default=str(Path(__file__).with_name("patch-only")))
    parser.add_argument("--tests", type=int, default=30)
    parser.add_argument("--n", type=int, default=100)
    parser.add_argument("--seed", type=int, default=419)
    parser.add_argument("--cache", default="cached-packings.pkl")
    args = parser.parse_args()
    cache = Path(__file__).with_name(args.cache)
    saved = {}
    if cache.exists():
        with cache.open("rb") as handle:
            saved = pickle.load(handle)
    catalog = free_polyominoes(Path(__file__).with_name("free-polyominoes.pkl"))
    rng = random.Random(args.seed)
    before, after, times = [], [], []
    wins = 0
    for case in range(args.tests):
        shapes = [rng.choice(catalog) for _ in range(args.n)]
        key = args.n, args.seed, case, args.base
        data = serialize(shapes)
        if key not in saved:
            result = subprocess.run([args.base], input=data, text=True, capture_output=True, timeout=15)
            assert result.returncode == 0, result.stderr
            saved[key] = result.stdout
            with cache.open("wb") as handle:
                pickle.dump(saved, handle)
        base = saved[key]
        before.append(validate(shapes, base))
        started = time.perf_counter()
        result = subprocess.run([args.optimizer], input=data + base, text=True, capture_output=True, timeout=15)
        times.append(time.perf_counter() - started)
        assert result.returncode == 0, result.stderr
        after.append(validate(shapes, result.stdout))
        wins += after[-1] > before[-1] + 1e-9
        assert after[-1] + 1e-9 >= before[-1], (case, before[-1], after[-1])
    print(f"n={args.n} tests={args.tests} wins={wins} "
          f"before={sum(before)/len(before):.8f} after={sum(after)/len(after):.8f} "
          f"mean_time={sum(times)/len(times):.4f}s max_time={max(times):.4f}s")


if __name__ == "__main__":
    main()
