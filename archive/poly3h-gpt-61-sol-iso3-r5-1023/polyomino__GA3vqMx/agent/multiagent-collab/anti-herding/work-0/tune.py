import argparse
import random
import subprocess
import time
from pathlib import Path

from test import all_shapes, input_text, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default=str(Path(__file__).parent / "solution"))
    parser.add_argument("--sizes", default="100,300,1000")
    parser.add_argument("--widths", default="12,16,20,24,28,32,40,48,56,62")
    parser.add_argument("--trials", default="96")
    parser.add_argument("--hole", default="26")
    parser.add_argument("--height", default="1.5")
    parser.add_argument("--cases", default=4, type=int)
    args = parser.parse_args()
    shapes = all_shapes()
    for n in map(int, args.sizes.split(",")):
        random.seed(123 + n)
        cases = [random.choices(shapes, k=n) for _ in range(args.cases)]
        data = [input_text(pieces) for pieces in cases]
        best = None
        for width in map(int, args.widths.split(",")):
            for trials in map(int, args.trials.split(",")):
                for hole in map(float, args.hole.split(",")):
                    for height in map(float, args.height.split(",")):
                        densities = []
                        start = time.perf_counter()
                        for pieces, text in zip(cases, data):
                            output = subprocess.run(
                                [args.binary, str(width), str(trials), str(hole), str(height)],
                                input=text, text=True, capture_output=True, check=True,
                            ).stdout
                            area = validate(pieces, output)
                            densities.append(sum(map(len, pieces)) / area)
                        seconds = (time.perf_counter() - start) / len(cases)
                        density = sum(densities) / len(densities)
                        print(f"n={n} width={width} trials={trials} hole={hole} height={height} density={density:.6f} seconds={seconds:.4f}", flush=True)
                        if best is None or density > best[0]:
                            best = (density, width, trials, hole, height, seconds)
        print("BEST", n, best, flush=True)


if __name__ == "__main__":
    main()
