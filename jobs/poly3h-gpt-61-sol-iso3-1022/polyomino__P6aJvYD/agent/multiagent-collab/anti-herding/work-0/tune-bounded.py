import argparse
from pathlib import Path
import random
import subprocess
import time

import test


root = Path("/logs/agent/multiagent-collab/anti-herding/work-0")
parser = argparse.ArgumentParser()
parser.add_argument("--exe", default=str(root / "bounded-compact-experiment"))
parser.add_argument("--baseline", default=str(root / "microexact-terminal-v11"))
parser.add_argument("--count", type=int, default=30)
parser.add_argument("--n", type=int, default=150)
parser.add_argument("--width", type=int, default=14)
parser.add_argument("--shrink", type=int, default=1)
parser.add_argument("--samples", type=int, default=128)
parser.add_argument("--exact", type=int, default=1024)
parser.add_argument("--beam", type=int, default=128)
parser.add_argument("--modes", default="classic32,classic128,0,1,2,3,5,7")
args = parser.parse_args()
data = iter(map(int, (root / "catalogue.txt").read_text().split()))
test.catalogue = [[(next(data), next(data)) for _ in range(next(data))]
                  for _ in range(next(data))]
rng = random.Random(71632)
fixtures = root / f"bounded-fixtures-n{args.n}-w{args.width}"
fixtures.mkdir(exist_ok=True)
cases = []
for index in range(args.count):
    pieces = test.generate(rng, args.n or (100, 150, 300)[index % 3], "uniform")
    text = test.encode(pieces)
    cached = fixtures / f"case-{index}.out"
    if cached.exists():
        output = cached.read_text()
    else:
        command = [args.baseline, str(args.width), "128", "4", "128", "0", "30",
                   "160", "-2", "0", "0", "1", "30", "0", "0", "0", "8", "1024"]
        result = subprocess.run(command, input=text, text=True, capture_output=True,
                                timeout=12, check=True)
        output = result.stdout
        cached.write_text(output)
    test.validate(pieces, output)
    width, height = sorted(map(int, output.split()[:2]))
    assert width == args.width, (width, height)
    cases.append((pieces, text, height - args.shrink, width * height))
for mode in args.modes.split(","):
    start = time.monotonic()
    success = 0
    density = 0
    details = []
    for index, (pieces, text, height, old_area) in enumerate(cases):
        if mode.startswith("classic"):
            options = [args.width, int(mode[7:]), 4, 30, 100, 60, 160, 2, height,
                       0, 1, 30, 8, 1, 100]
        else:
            options = [args.width, args.beam, 4, args.samples, 0, 30, 160, -2, height,
                       0, 1, 30, 0, 0, 0, 8, args.exact, 6, int(mode)]
        command = [args.exe, *map(str, options)]
        result = subprocess.run(command, input=text, text=True, capture_output=True,
                                timeout=12, check=True)
        dimensions = list(map(int, result.stdout.split()[:2]))
        if dimensions == [0, 0]:
            assert len(result.stdout.split()) == 2, result.stdout
            new_area = old_area
        else:
            test.validate(pieces, result.stdout)
            new_area = dimensions[0] * dimensions[1]
            assert new_area <= args.width * height
            success += 1
            details.append(index)
        density += sum(map(len, pieces)) / new_area
    print(f"mode={mode} solved={success}/{len(cases)} density={density/len(cases):.7f} "
          f"seconds={time.monotonic()-start:.3f} cases={details}", flush=True)
