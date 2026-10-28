import pickle
import random
import resource
import subprocess
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def main():
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(87051491)
    cases = []
    for n in (100, 100, 100, 100, 150, 150, 150, 150, 300, 300, 300, 300):
        pieces = [rng.choice(shapes) for _ in range(n)]
        cases.append((pieces, encode(pieces)))
    policies = [
        ("best96167", "/logs/agent/multiagent-collab/anti-herding/work-0/fastwindow", []),
        ("beam16", str(BASE / "beam"), ["16", "3"]),
        ("beam32", str(BASE / "beam"), ["32", "3"]),
        ("beam16air0", str(BASE / "beam"), ["16", "3", "0", ".75", ".025"]),
        ("beam32air0", str(BASE / "beam"), ["32", "3", "0", ".75", ".025"]),
        ("beam16p0", str(BASE / "beam"), ["16", "3", "0", ".75", "0"]),
    ]
    for name, executable, arguments in policies:
        densities = []
        cpu_begin = resource.getrusage(resource.RUSAGE_CHILDREN)
        for pieces, text in cases:
            proc = subprocess.run([executable, "0", ".8", *arguments], input=text,
                                  text=True, stdout=subprocess.PIPE, check=True, timeout=5)
            area = validate(pieces, proc.stdout)
            densities.append(sum(map(len, pieces)) / area)
        cpu_end = resource.getrusage(resource.RUSAGE_CHILDREN)
        used = cpu_end.ru_utime + cpu_end.ru_stime - cpu_begin.ru_utime - cpu_begin.ru_stime
        print(f"policy={name} mean={sum(densities)/len(densities):.8f} cpu={used:.4f} "
              f"small={sum(densities[:8])/8:.8f} "
              f"cases={','.join(f'{x:.5f}' for x in densities)}", flush=True)


if __name__ == "__main__":
    main()
