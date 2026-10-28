import argparse
from pathlib import Path
import random
import subprocess
import time
import shlex
from test import validate

root = Path("/logs/agent/multiagent-collab/anti-herding/work-0")
parser = argparse.ArgumentParser()
parser.add_argument("--ms", type=int, default=100)
parser.add_argument("--modes", default="0,1,2,3,4,5,6,7")
parser.add_argument("--count", type=int, default=18)
parser.add_argument("--algorithm", default="replay")
parser.add_argument("--baseline", default="champion-peer-v6")
parser.add_argument("--schedule", default="")
parser.add_argument("--seed", type=int, default=73851)
parser.add_argument("--sizes", default="100,150,300")
parser.add_argument("--exe", default=str(root / "solution"))
args = parser.parse_args()
data = iter(map(int, (root / "catalogue.txt").read_text().split()))
catalogue = []
for _ in range(next(data)):
    catalogue.append([(next(data), next(data)) for _ in range(next(data))])
rng = random.Random(args.seed)
suffix = "" if args.seed == 73851 and args.sizes == "100,150,300" else f"-{args.seed}-{args.sizes}"
fixtures = root / (("fixtures" if args.baseline == "champion-peer-v6" else "fixtures-" + args.baseline) + suffix)
fixtures.mkdir(exist_ok=True)
sizes = list(map(int, args.sizes.split(",")))
cases = []
for index in range(args.count):
    n = sizes[index % len(sizes)]
    cells = [rng.choice(catalogue) for _ in range(n)]
    lines = [str(n)]
    for shape in cells:
        lines.append(str(len(shape)))
        lines.extend(f"{x} {y}" for x, y in shape)
    text = "\n".join(lines) + "\n"
    before = fixtures / f"case-{index}.out"
    if not before.exists():
        result = subprocess.run([str(root / args.baseline)], input=text, text=True,
                                capture_output=True, check=True)
        before.write_text(result.stdout)
    output = before.read_text()
    width, height = map(int, output.split()[:2])
    area = sum(map(len, cells))
    validate(cells, output)
    cases.append((text + output, area, width * height, cells))
start = time.monotonic()
for mode in map(int, args.modes.split(",")):
    wins = 0
    losses = 0
    mean = 0.0
    before = 0.0
    saved = 0
    details = []
    for index, (text, area, old_area, cells) in enumerate(cases):
        command = [args.exe]
        if args.algorithm != "default":
            command.extend([args.algorithm, str(args.ms), str(mode)])
        elif args.schedule:
            command.extend(shlex.split(args.schedule.format(mode=mode)))
        result = subprocess.run(command,
                                input=text, text=True, capture_output=True, check=True)
        w, h = map(int, result.stdout.split()[:2])
        validate(cells, result.stdout)
        new_area = w * h
        wins += new_area < old_area
        losses += new_area > old_area
        mean += area / new_area
        before += area / old_area
        saved += old_area - new_area
        if new_area < old_area:
            details.append((index, old_area - new_area, result.stderr.strip()))
    print(f"mode={mode} wins={wins}/{len(cases)} losses={losses} density={mean/len(cases):.7f} "
          f"before={before/len(cases):.7f} cells_saved={saved} details={details}", flush=True)
print(f"seconds={time.monotonic()-start:.3f}")
