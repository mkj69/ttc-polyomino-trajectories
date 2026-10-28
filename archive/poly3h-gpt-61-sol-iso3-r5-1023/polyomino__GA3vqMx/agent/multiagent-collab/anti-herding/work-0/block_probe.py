import argparse
import random
import resource
import subprocess

from test import all_shapes, input_text, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=int, default=3)
    parser.add_argument("--sizes", default="100,300,1000,10000")
    parser.add_argument("--blocks", default="6x5,8x5,6x8,10x6")
    parser.add_argument("--seconds", default="0.5")
    parser.add_argument("--nodes", default="5000")
    parser.add_argument("--columns", default="3")
    args = parser.parse_args()
    shapes = all_shapes()
    for n in map(int, args.sizes.split(",")):
        for seed in range(args.cases):
            random.seed(n * 100 + seed)
            pieces = random.choices(shapes, k=n)
            total = sum(map(len, pieces))
            data = input_text(pieces)
            for block in args.blocks.split(","):
                width, height = block.split("x")
                before = resource.getrusage(resource.RUSAGE_CHILDREN)
                process = subprocess.run(
                    ["./blocks", width, height, args.columns, args.seconds, args.nodes],
                    input=data, text=True, capture_output=True, check=True,
                )
                after = resource.getrusage(resource.RUSAGE_CHILDREN)
                area = validate(pieces, process.stdout) if process.stdout.strip() else None
                cpu = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
                print("n", n, "seed", seed, "block", block, "area", area,
                      "density", total / area if area else None, "cpu", round(cpu, 4),
                      process.stderr.strip(), flush=True)


if __name__ == "__main__":
    main()
