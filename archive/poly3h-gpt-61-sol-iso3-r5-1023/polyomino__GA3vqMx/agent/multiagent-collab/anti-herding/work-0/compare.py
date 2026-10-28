import argparse
import random
import resource
import subprocess
import time

from test import all_shapes, input_text, validate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binaries", nargs="+")
    parser.add_argument("--sizes", default="100,300,1000,10000")
    parser.add_argument("--cases", default=3, type=int)
    parser.add_argument("--width", type=int)
    args = parser.parse_args()
    shapes = all_shapes()
    for n in map(int, args.sizes.split(",")):
        stats = [[] for _ in args.binaries]
        for seed in range(args.cases):
            random.seed(n * 100 + seed)
            pieces = random.choices(shapes, k=n)
            data = input_text(pieces)
            areas = []
            seconds = []
            cpu_seconds = []
            for index, binary in enumerate(args.binaries):
                start = time.perf_counter()
                before = resource.getrusage(resource.RUSAGE_CHILDREN)
                command = [binary] + ([str(args.width)] if args.width else [])
                output = subprocess.run(command, input=data, text=True,
                                        capture_output=True, check=True).stdout
                seconds.append(time.perf_counter() - start)
                after = resource.getrusage(resource.RUSAGE_CHILDREN)
                cpu_seconds.append(after.ru_utime + after.ru_stime -
                                   before.ru_utime - before.ru_stime)
                area = validate(pieces, output)
                areas.append(area)
                stats[index].append(sum(map(len, pieces)) / area)
            print("n", n, "seed", seed, "areas", areas, "seconds", seconds,
                  "cpu_seconds", cpu_seconds, flush=True)
        print("MEAN", n, [sum(values) / len(values) for values in stats], flush=True)


if __name__ == "__main__":
    main()
