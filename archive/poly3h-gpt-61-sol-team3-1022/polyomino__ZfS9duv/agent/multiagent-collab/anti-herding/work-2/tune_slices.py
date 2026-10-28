import argparse
import os
import pickle
import random
import re
import resource
import subprocess
from pathlib import Path

from check import encode, validate

BASE = Path("/logs/agent/multiagent-collab/anti-herding/work-2")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--san", action="store_true")
    args = parser.parse_args()
    shapes = pickle.loads((BASE / "catalogue.pickle").read_bytes())
    rng = random.Random(139970608)
    sizes = (100, 150, 300, 500, 1000, 1000) * 2
    settings = ((0, 16, 64), (.15, 16, 64), (.15, 32, 32), (.15, 64, 16))
    if args.san:
        sizes = (100, 150, 300, 500, 1000) * 12
        settings = ((.06, 8, 24),)
    cases = []
    for n in sizes:
        pieces = []
        for _ in range(n):
            ox, oy = rng.randint(-10**12, 10**12), rng.randint(-10**12, 10**12)
            pieces.append([(x + ox, y + oy) for x, y in rng.choice(shapes)])
        cases.append((pieces, encode(pieces)))
    for budget, breadth, span in settings:
        densities = []
        attempts = gains = 0
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        for pieces, text in cases:
            large = len(pieces) > 500
            binary = "beam_slices_san" if args.san else "beam_slices"
            proc = subprocess.run(
                [str(BASE / binary), "0", ".05" if args.san else ".8", "12",
                 "-1", "-1", "1", "-1", "256", "0", "10000",
                 "4" if large else "32", "96", "1" if large else "0",
                 "0" if large else ".05", ".35", "0", "20", "1", "4", "3",
                 "1", "1.1", str(budget), str(breadth), str(span)],
                input=text, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                env=dict(os.environ, PACK_DEBUG="1"), check=True, timeout=15)
            area = validate(pieces, proc.stdout)
            densities.append(sum(map(len, pieces)) / area)
            match = re.search(r"slices attempts=(\d+) gains=(\d+)", proc.stderr)
            if match:
                attempts += int(match.group(1))
                gains += int(match.group(2))
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        used = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
        print(f"san={args.san} budget={budget} B={breadth} span={span} "
              f"mean={sum(densities)/len(densities):.8f} actual_gains={gains}/{attempts} "
              f"cpu={used:.3f} cases={','.join(f'{x:.5f}' for x in densities)}", flush=True)


if __name__ == "__main__":
    main()
