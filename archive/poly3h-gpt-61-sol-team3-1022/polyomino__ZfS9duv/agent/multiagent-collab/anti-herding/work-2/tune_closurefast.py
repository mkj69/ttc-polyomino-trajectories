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
    parser.add_argument("--fixed", action="store_true")
    parser.add_argument("--binary", default="beam_closure_fast")
    args = parser.parse_args()
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(779415038)
    cases = []
    sizes = (100, 150, 300, 500, 1000, 3000) * (4 if args.fixed else 2)
    for n in sizes:
        pieces = [rng.choice(shapes) for _ in range(n)]
        cases.append((pieces, encode(pieces)))
    configurations = ((0, 8, 96, 512), (4, 8, 96, 512), (8, 4, 0, 1024),
                      (8, 6, 0, 1024), (4, 8, 0, 2048))
    reference = []
    for variant, (states, tail, cap, nodes) in enumerate(configurations):
        densities = []
        wins = losses = 0
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        for index, (pieces, text) in enumerate(cases):
            large = len(pieces) > 500
            executable = args.binary + ("_fixed" if args.fixed else "")
            width = str((20, 31, 40)[index // 6 % 3]) if args.fixed else "0"
            proc = subprocess.run(
                [str(BASE / executable), width, "1" if args.fixed else "1.5", "12",
                 "-1", "-1", "1", "-1", "256", "0", "10000",
                 "4" if large else "32", "96", "1" if large else "0",
                 "0" if large else ".05", ".35", "0", "20", "1", "4", "3",
                 str(states), str(tail), str(cap), str(nodes)],
                input=text, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, check=True, timeout=15)
            area = validate(pieces, proc.stdout)
            densities.append(sum(map(len, pieces)) / area)
            if not variant:
                reference.append(area)
            else:
                wins += area < reference[index]
                losses += area > reference[index]
                if args.fixed:
                    assert area <= reference[index], (index, states, tail, area, reference[index])
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        used = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
        print(f"fixed={args.fixed} states={states} tail={tail} cap={cap} nodes={nodes} "
              f"mean={sum(densities)/len(densities):.8f} wins={wins} losses={losses} "
              f"cpu={used:.3f} cases={','.join(f'{x:.5f}' for x in densities)}", flush=True)


if __name__ == "__main__":
    main()
