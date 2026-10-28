import argparse
import random
import subprocess

from test import all_shapes, input_text, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=int, default=10)
    parser.add_argument("--sizes", default="100,300,1000")
    parser.add_argument("--weights", default="0,3,6,12,24,-6")
    parser.add_argument("--widths", default="16,24,32")
    args = parser.parse_args()
    shapes = all_shapes()
    weights = args.weights.split(",")
    for n in map(int, args.sizes.split(",")):
        stats = [[] for _ in weights]
        for seed in range(args.cases):
            random.seed(n * 100 + seed)
            pieces = random.choices(shapes, k=n)
            data = input_text(pieces)
            results = []
            for weight in weights:
                best = None
                for width in args.widths.split(","):
                    process = subprocess.run(
                        ["./regret", width, "96", "26", "1.5", weight],
                        input=data, text=True, capture_output=True, check=True,
                    )
                    area = validate(pieces, process.stdout)
                    best = min(best, area) if best else area
                results.append(sum(map(len, pieces)) / best)
            for stat, result in zip(stats, results):
                stat.append(result)
            print("n", n, "seed", seed, "densities", results, flush=True)
        print("MEAN", n, list(zip(weights, [sum(stat) / len(stat) for stat in stats])),
              flush=True)


if __name__ == "__main__":
    main()
