import pickle
import random
import resource
import subprocess
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def main():
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(410039741)
    cases = []
    for n in (100, 100, 150, 150, 300, 300, 500, 500):
        pieces = [rng.choice(shapes) for _ in range(n)]
        cases.append((pieces, encode(pieces)))
    for policy, duration in ((1, .08), (9, .02), (9, .05), (9, .1), (9, .2)):
        densities = []
        cpu_begin = resource.getrusage(resource.RUSAGE_CHILDREN)
        for pieces, text in cases:
            proc = subprocess.run(
                [str(BASE / "chokudai"), "0", ".8", "12", "-1", "-1", "1",
                 "-1", "256", "0", "500", "32", "96", "3", ".75", ".025",
                 "-1", ".75", str(policy), str(duration)],
                input=text, text=True, stdout=subprocess.PIPE, check=True, timeout=7)
            area = validate(pieces, proc.stdout)
            densities.append(sum(map(len, pieces)) / area)
        cpu_end = resource.getrusage(resource.RUSAGE_CHILDREN)
        used = cpu_end.ru_utime + cpu_end.ru_stime - cpu_begin.ru_utime - cpu_begin.ru_stime
        print(f"policy={policy} local_budget={duration} "
              f"mean={sum(densities)/len(densities):.8f} cpu={used:.3f} "
              f"cases={','.join(f'{x:.5f}' for x in densities)}", flush=True)


if __name__ == "__main__":
    main()
