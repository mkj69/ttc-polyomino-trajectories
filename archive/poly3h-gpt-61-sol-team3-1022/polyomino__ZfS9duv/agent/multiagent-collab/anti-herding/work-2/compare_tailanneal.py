import pickle
import random
import resource
import subprocess
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def main():
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(98310067)
    cases = []
    for n in (100, 100, 150, 150, 300, 300, 500, 500):
        pieces = [rng.choice(shapes) for _ in range(n)]
        cases.append((pieces, encode(pieces)))
    for budget, states, tail, mode in ((0, 0, 8, 0), (.0005, 2, 8, 0),
                                      (.002, 2, 8, 4), (.002, 2, 12, 4),
                                      (.002, 1, 16, 20), (.005, 1, 16, 20),
                                      (.005, 1, 20, 20), (.002, 2, 12, 36)):
        densities = []
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        for pieces, text in cases:
            proc = subprocess.run(
                [str(BASE / "beam_tailanneal"), "0", ".8", "12", "-1", "-1",
                 "1", "-1", "256", "0", "500", "32", "96", "3", ".75", ".025",
                 "-1", ".75", "1", str(budget), str(states), str(tail), str(mode)],
                input=text, text=True, stdout=subprocess.PIPE, check=True, timeout=7)
            area = validate(pieces, proc.stdout)
            densities.append(sum(map(len, pieces)) / area)
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        used = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
        print(f"budget={budget} states={states} tail={tail} mode={mode} "
              f"mean={sum(densities)/len(densities):.8f} cpu={used:.3f} "
              f"cases={','.join(f'{x:.5f}' for x in densities)}", flush=True)


if __name__ == "__main__":
    main()
