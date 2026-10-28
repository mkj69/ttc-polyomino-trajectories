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
    parser.add_argument("--large", action="store_true")
    parser.add_argument("--persistent", action="store_true")
    parser.add_argument("--count-only", action="store_true")
    args = parser.parse_args()
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(16923005)
    sizes = (100, 200, 350, 500) * 10 if args.fixed else (
        600, 600, 1000, 1000, 2000, 2000, 5000, 10000)
    if args.large:
        sizes = (600, 1000, 2000, 5000) * 3
    cases = []
    for n in sizes:
        pieces = [rng.choice(shapes) for _ in range(n)]
        cases.append((pieces, encode(pieces)))
    variants = (("frontier_reference", 32, 96, 0), ("frontier_fixed", 32, 96, 0)) if args.fixed else (
        ("/app/solution", 0, 0, 0), ("beam_frontier", 2, 32, 1),
        ("beam_frontier", 4, 48, 1), ("beam_frontier", 8, 32, 1),
        ("beam_frontier", 4, 96, 1), ("beam_frontier", 8, 96, 1))
    if args.large:
        variants = (("frontier_vector_fixed", 4, 96, 1), ("frontier_fixed", 4, 96, 1))
    if args.count_only:
        variants = (("frontier_fixed", 4, 96, 1),) * 2
    reference = []
    for variant, (binary, beam, fits, policy) in enumerate(variants):
        densities = []
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        for index, (pieces, text) in enumerate(cases):
            executable = str(BASE / binary) if not binary.startswith("/") else binary
            command = [executable]
            if args.fixed:
                command += ["20", "1", "12", "-1", "-1", "1", "-1", "256",
                            "0", "10000", str(beam), str(fits), str(policy)]
            elif beam:
                command += ["0", "1.5", "12", "-1", "-1", "1", "-1", "256",
                            "0", "10000", str(beam), str(fits), str(policy)]
            if args.persistent:
                command += ["0", "0"]
            elif args.count_only:
                command += ["0", "10000" if variant == 0 else "0"]
            proc = subprocess.run(command, input=text, text=True,
                                  stdout=subprocess.PIPE, check=True, timeout=12)
            area = validate(pieces, proc.stdout)
            densities.append(sum(map(len, pieces)) / area)
            if args.fixed:
                if variant == 0:
                    reference.append(proc.stdout)
                else:
                    assert reference[index] == proc.stdout, index
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        used = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
        print(f"{binary} B={beam} fits={fits} mean={sum(densities)/len(densities):.8f} "
              f"cpu={used:.3f} cases={','.join(f'{x:.5f}' for x in densities)}",
              flush=True)


if __name__ == "__main__":
    main()
