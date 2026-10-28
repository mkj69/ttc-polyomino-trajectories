import argparse
import random
import subprocess

from test import all_shapes, input_text, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cases", type=int, default=3)
    parser.add_argument("--sizes", default="100")
    parser.add_argument("--densities", default="0.90,0.94,0.96")
    parser.add_argument("--widths", default="16,24")
    parser.add_argument("--modes", default="0,1")
    parser.add_argument("--seconds", default="0.5")
    parser.add_argument("--nodes", default="500")
    parser.add_argument("--jitter", default="5")
    parser.add_argument("--area", default="3")
    args = parser.parse_args()
    shapes = all_shapes()
    for n in map(int, args.sizes.split(",")):
        for seed in range(args.cases):
            random.seed(n * 100 + seed)
            pieces = random.choices(shapes, k=n)
            total = sum(map(len, pieces))
            data = input_text(pieces)
            for density in map(float, args.densities.split(",")):
                for width in map(int, args.widths.split(",")):
                    height = int((total / density + width - 1) // width)
                    for mode in args.modes.split(","):
                        process = subprocess.run(
                            ["./rectangle", str(width), str(height), mode, args.seconds,
                             args.nodes, args.jitter, args.area],
                            input=data, text=True, capture_output=True, check=True,
                        )
                        area = validate(pieces, process.stdout) if process.stdout.strip() else None
                        print("n", n, "seed", seed, "target", density,
                              "width", width, "mode", mode, "area", area,
                              process.stderr.strip(), flush=True)


if __name__ == "__main__":
    main()
