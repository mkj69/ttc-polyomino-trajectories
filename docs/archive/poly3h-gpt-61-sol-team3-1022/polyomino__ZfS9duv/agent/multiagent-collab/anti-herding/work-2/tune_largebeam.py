import argparse
import pickle
import random
import resource
import subprocess
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("quality", "widths", "persistent"),
                        default="quality")
    args = parser.parse_args()
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(92643019)
    cases = []
    for n in (600, 600, 1000, 1000, 2000, 2000, 5000, 10000):
        pieces = [rng.choice(shapes) for _ in range(n)]
        cases.append((pieces, encode(pieces)))
    modes = ((4, 96, 40, 10000), (4, 192, 40, 10000), (4, 384, 40, 10000),
             (8, 96, 40, 10000), (2, 192, 40, 10000), (16, 48, 40, 10000))
    if args.mode == "widths":
        modes = ((4, 96, width, 10000) for width in (10, 15, 20, 25, 30, 35, 40, 50))
    elif args.mode == "persistent":
        modes = ((4, 96, 40, gate) for gate in (10000, 0, 500, 1500))
    for beam, fits, width, gate in modes:
        densities = []
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        for pieces, text in cases:
            proc = subprocess.run(
                [str(BASE / "beam_persistent"), "0", "1.5", "12", "-1", "-1",
                 "1", "-1", "256", "0", "10000", str(beam), str(fits), "1",
                 str(width), str(gate)],
                input=text, text=True, stdout=subprocess.PIPE, check=True, timeout=12)
            area = validate(pieces, proc.stdout)
            densities.append(sum(map(len, pieces)) / area)
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        used = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
        print(f"beam={beam} fits={fits} width={width} gate={gate} "
              f"mean={sum(densities)/len(densities):.8f} cpu={used:.3f} "
              f"cases={','.join(f'{x:.5f}' for x in densities)}", flush=True)


if __name__ == "__main__":
    main()
