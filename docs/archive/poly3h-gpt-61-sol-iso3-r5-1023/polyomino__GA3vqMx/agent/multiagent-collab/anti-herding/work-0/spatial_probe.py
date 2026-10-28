import argparse
from pathlib import Path
import random
import resource
import subprocess

from test import all_shapes, input_text, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", default="./spatial-baseline-internal")
    parser.add_argument("--probe", default="./spatial-only")
    parser.add_argument("--sizes", default="100,150,250,400,700")
    parser.add_argument("--cases", type=int, default=4)
    parser.add_argument("--seconds", default="0.3")
    parser.add_argument("--cache", default="spatial-cases")
    args = parser.parse_args()
    shapes = all_shapes()
    for n in map(int, args.sizes.split(",")):
        densities = [[], []]
        for seed in range(args.cases):
            random.seed(n * 100 + seed)
            pieces = random.choices(shapes, k=n)
            data = input_text(pieces)
            cached = Path(args.cache) / f"n{n}-seed{seed}.out"
            cached.parent.mkdir(parents=True, exist_ok=True)
            if cached.exists():
                baseline = cached.read_text()
            else:
                baseline = subprocess.run(
                    [args.baseline], input=data, text=True, capture_output=True, check=True
                ).stdout
                cached.write_text(baseline)
            before = resource.getrusage(resource.RUSAGE_CHILDREN)
            result = subprocess.run(
                [args.probe, args.seconds], input=data + baseline, text=True,
                capture_output=True
            )
            if result.returncode:
                raise RuntimeError(result.stderr)
            after = resource.getrusage(resource.RUSAGE_CHILDREN)
            areas = [validate(pieces, text) for text in (baseline, result.stdout)]
            for i, area in enumerate(areas):
                densities[i].append(sum(map(len, pieces)) / area)
            print("n", n, "seed", seed, "areas", areas,
                  "cpu", after.ru_utime + after.ru_stime -
                  before.ru_utime - before.ru_stime, result.stderr.strip(), flush=True)
        print("MEAN", n, [sum(values) / len(values) for values in densities], flush=True)


if __name__ == "__main__":
    main()
