import pickle
import random
import resource
import subprocess
import sys
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")
DEFAULT = [32, 96, 3, .75, .025, -1, .75]


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "parameters"
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(789225714 if mode != "large" else 331782930)
    sizes = (100, 100, 150, 150, 300, 300, 500, 500)
    if mode == "large":
        sizes = (600, 600, 1000, 1000, 1500, 1500)
    cases = []
    for n in sizes:
        pieces = [rng.choice(shapes) for _ in range(n)]
        cases.append((pieces, encode(pieces)))
    variations = [("baseline", DEFAULT)]
    dimensions = {0: (8, 16, 24, 48, 64), 1: (24, 48, 64, 128, 256),
                  2: (2, 4, 5), 3: (0, .25, 1.5, 3),
                  4: (.01, .05, .1), 5: (6, 8, 10, 12, 14),
                  6: (.5, 1, 1.25)}
    if mode == "large":
        dimensions = {0: (4, 8, 16), 1: (48, 128), 6: (.5, 1)}
    if mode == "weights":
        dimensions = {3: (0, .25, 1.5, 3), 4: (.01, .05, .1),
                      5: (6, 8, 10, 12, 14), 6: (.5, 1, 1.25)}
    if mode == "parameters":
        dimensions = {0: (8, 16, 24, 48, 64), 1: (24, 48, 64, 128, 256),
                      2: (2, 4, 5)}
    if mode == "adaptive":
        dimensions = {}
        variations = [(f"adaptive{policy}", [*DEFAULT, policy])
                      for policy in (0, 1, 2, 3, 4, 6)]
    for index, values in dimensions.items():
        for value in values:
            config = DEFAULT.copy()
            config[index] = value
            variations.append((f"{index}={value}", config))
    duration = 1.5 if mode == "large" else .6
    for name, config in variations:
        densities = []
        cpu_begin = resource.getrusage(resource.RUSAGE_CHILDREN)
        for pieces, text in cases:
            proc = subprocess.run(
                [str(BASE / "beam_tune"), "0", str(duration), "12", "-1", "-1",
                 "1", "-1", "256", "0", "2000", *map(str, config)],
                input=text, text=True, stdout=subprocess.PIPE, check=True, timeout=7)
            area = validate(pieces, proc.stdout)
            densities.append(sum(map(len, pieces)) / area)
        cpu_end = resource.getrusage(resource.RUSAGE_CHILDREN)
        used = cpu_end.ru_utime + cpu_end.ru_stime - cpu_begin.ru_utime - cpu_begin.ru_stime
        print(f"{name} cfg={config} mean={sum(densities)/len(densities):.8f} "
              f"cpu={used:.3f} cases={','.join(f'{x:.5f}' for x in densities)}",
              flush=True)


if __name__ == "__main__":
    main()
