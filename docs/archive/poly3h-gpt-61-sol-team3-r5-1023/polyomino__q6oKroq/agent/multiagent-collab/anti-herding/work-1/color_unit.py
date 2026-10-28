import importlib.util
import random
import subprocess
import sys

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
rng = random.Random(812651)
inputs, expected = [], []
for case in range(300):
    width, height = rng.randint(1, 5), rng.randint(1, 4)
    pieces = checks.generate(782911 + case, rng.randint(1, 5), "small")
    rows = []
    for index, piece in enumerate(pieces):
        masks = set()
        for flip in range(2):
            for rotation in range(4):
                cells = []
                for x, y in piece:
                    if flip:
                        x = -x
                    for _ in range(rotation):
                        x, y = y, -x
                    cells.append((x, y))
                minx, miny = min(x for x, y in cells), min(y for x, y in cells)
                cells = [(x - minx, y - miny) for x, y in cells]
                maxx, maxy = max(x for x, y in cells), max(y for x, y in cells)
                for y in range(height - maxy):
                    for x in range(width - maxx):
                        masks.add(sum(1 << ((cy + y) * width + cx + x) for cx, cy in cells))
        rows.extend((index, mask) for mask in masks)
    inputs.append(f"{width} {height} {len(pieces)} {len(rows)}\n" +
                  "".join(f"{piece} {mask}\n" for piece, mask in rows))
    expected.append(checks.brute_feasible(pieces, width, height))
proc = subprocess.run([sys.argv[1]], input=str(len(inputs)) + "\n" + "".join(inputs),
                      capture_output=True, text=True, timeout=60, check=True)
for i, (line, answer) in enumerate(zip(proc.stdout.splitlines(), expected)):
    results = list(map(int, line.split()))
    assert len(results) == 4 and all(result == answer for result in results), (i, results, answer)
assert len(proc.stdout.splitlines()) == len(expected)
print("300 tiny feasibility instances x4 color modes match brute force",
      "feasible", sum(expected), "infeasible", len(expected) - sum(expected))
