import pickle
import random
import resource
import subprocess
import sys
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def main():
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(47118253)
    cases = []
    for n in (100, 100, 100, 100, 300, 300, 300, 300):
        pieces = [rng.choice(shapes) for _ in range(n)]
        cases.append((pieces, encode(pieces)))
    settings = [(16, 3, air, sealed, .025)
                for air in (0, .03, .08, .2, .5)
                for sealed in (0, .75, 2)]
    settings += [(beam, branch, .08, .75, perim)
                 for beam, branch in ((4, 3), (8, 3), (32, 3), (64, 3), (16, 5))
                 for perim in (0, .1)]
    if len(sys.argv) > 1:
        settings = [(16, 3, .08, .75, .025), (8, 3, .08, .75, .025),
                    (4, 3, .08, .75, .025), (32, 3, .08, .75, .025)]
    for beam, branch, air, sealed, perim in settings:
        densities = []
        cpu_begin = resource.getrusage(resource.RUSAGE_CHILDREN)
        for pieces, text in cases:
            arguments = ["0", ".4"] if len(sys.argv) > 1 else ["20", "1.5"]
            proc = subprocess.run(
                [str(BASE / "beam"), *arguments, str(beam), str(branch),
                 str(air), str(sealed), str(perim)], input=text, text=True,
                stdout=subprocess.PIPE, check=True, timeout=5)
            area = validate(pieces, proc.stdout)
            densities.append(sum(map(len, pieces)) / area)
        cpu_end = resource.getrusage(resource.RUSAGE_CHILDREN)
        used = cpu_end.ru_utime + cpu_end.ru_stime - cpu_begin.ru_utime - cpu_begin.ru_stime
        print(f"beam={beam} branch={branch} air={air} sealed={sealed} perim={perim} "
              f"mean={sum(densities)/len(densities):.8f} cpu={used:.4f} "
              f"cases={','.join(f'{x:.5f}' for x in densities)}", flush=True)


if __name__ == "__main__":
    main()
