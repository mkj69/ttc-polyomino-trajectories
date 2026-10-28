import pickle
import random
import resource
import subprocess
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def main():
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(762619403)
    cases = []
    for n in (100, 100, 150, 150, 300, 300, 500, 500, 1000, 1000, 3000, 3000):
        pieces = [rng.choice(shapes) for _ in range(n)]
        cases.append((pieces, encode(pieces)))
    for states, tail, cap, nodes in ((0, 4, 32, 256), (4, 4, 32, 256),
                                   (8, 4, 64, 512), (4, 6, 64, 512),
                                   (4, 8, 96, 512), (8, 4, 0, 1024)):
        densities = []
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        for pieces, text in cases:
            large = len(pieces) > 500
            proc = subprocess.run(
                [str(BASE / "beam_closure"), "0", "1.2", "12", "-1", "-1", "1",
                 "-1", "256", "0", "10000", "4" if large else "32", "96",
                 "1" if large else "0", "0" if large else ".05", "1", "4",
                 "3", str(states), str(tail), str(cap), str(nodes)],
                input=text, text=True, stdout=subprocess.PIPE, check=True, timeout=12)
            area = validate(pieces, proc.stdout)
            densities.append(sum(map(len, pieces)) / area)
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        used = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
        print(f"states={states} tail={tail} cap={cap} nodes={nodes} "
              f"mean={sum(densities)/len(densities):.8f} "
              f"small={sum(densities[:8])/8:.8f} large={sum(densities[8:])/4:.8f} "
              f"cpu={used:.3f} cases={','.join(f'{x:.5f}' for x in densities)}", flush=True)


if __name__ == "__main__":
    main()
