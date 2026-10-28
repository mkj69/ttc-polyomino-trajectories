import argparse
import importlib.util
import random
import time

spec = importlib.util.spec_from_file_location(
    "peer_tests", "/logs/agent/multiagent-collab/anti-herding/work-0/test.py")
tests = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tests)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("executables", nargs="+")
    parser.add_argument("--counts", default="100,200,500,1000,2000,4000,10000")
    parser.add_argument("--repeat", type=int, default=2)
    parser.add_argument("--seed", type=int, default=18461)
    args = parser.parse_args()
    rng = random.Random(args.seed)
    shapes = tests.all_shapes()
    for n in map(int, args.counts.split(",")):
        sums = [[0.0, 0.0] for _ in args.executables]
        for rep in range(args.repeat):
            pieces = rng.choices(shapes, k=n)
            data = tests.input_text(pieces)
            for i, exe in enumerate(args.executables):
                started = time.monotonic()
                process = tests.subprocess.run(
                    [exe], input=data, text=True, capture_output=True, check=True)
                elapsed = time.monotonic() - started
                area = tests.validate(pieces, process.stdout)
                density = sum(map(len, pieces)) / area
                sums[i][0] += density / args.repeat
                sums[i][1] += elapsed / args.repeat
        print("n", n, *[f"{exe} density={value[0]:.6f} time={value[1]:.3f}"
                        for exe, value in zip(args.executables, sums)], flush=True)


if __name__ == "__main__":
    main()
