import pickle
import random
import subprocess
import time
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def main():
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(2196508)
    cases = []
    for n in (100, 100, 100, 300, 300, 1000):
        pieces = [rng.choice(shapes) for _ in range(n)]
        cases.append((pieces, encode(pieces)))
    for tail in (4, 6, 8, 10, 12, 16, 20, 24, 32):
        for duration in (.001, .005, .02, .08):
            elapsed = 0
            densities, wins = [], 0
            for pieces, text in cases:
                begin = time.monotonic()
                proc = subprocess.run(
                    [str(BASE / "exact"), "30", ".15", str(tail), str(duration)],
                    input=text, text=True, stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE, check=True, timeout=5)
                elapsed += time.monotonic() - begin
                area = validate(pieces, proc.stdout)
                densities.append(sum(map(len, pieces)) / area)
            print(f"tail={tail} duration={duration} mean={sum(densities)/len(densities):.8f} "
                  f"n100mean={sum(densities[:3])/3:.8f} "
                  f"taildens={','.join(f'{x:.5f}' for x in densities)} "
                  f"elapsed={elapsed:.5f}", flush=True)


if __name__ == "__main__":
    main()
