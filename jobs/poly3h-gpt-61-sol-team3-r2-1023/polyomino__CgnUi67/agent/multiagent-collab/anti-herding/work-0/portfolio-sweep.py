import subprocess
import sys
import time

sys.path.insert(0, "/logs/agent/multiagent-collab/anti-herding/work-0")
from check import generate, validate

EXE = "/app/solution"
configs = [
    ("archive-only", 650, 0, 1, 250, 0),
    ("wide650-c4", 650, 4, 1, 250, 0),
    ("wide650-c3", 650, 3, 1, 250, 0),
    ("wide650-c2", 650, 2, 1, 250, 0),
    ("narrow650-c4", 650, 4, 1, 250, 1),
    ("wide350-c4", 350, 4, 1, 250, 0),
    ("narrow350-c4", 350, 4, 1, 250, 1),
    ("narrow650-c0", 650, 0, 1, 250, 1),
    ("archive2-c4", 650, 4, 2, 250, 0),
    ("wide650-fast", 650, 4, 1, 150, 0),
    ("narrow350-c3", 350, 3, 1, 250, 1),
]
cases = [(100, 81), (100, 82), (100, 83), (200, 81), (200, 82), (400, 81)]
inputs = []
for n, seed in cases:
    pieces = generate(n, seed, "uniform")
    text = str(n) + "\n" + "".join(
        str(len(p)) + "\n" + "".join(f"{x} {y}\n" for x, y in p)
        for p in pieces)
    inputs.append((pieces, text))
for name, *args in configs:
    densities = []
    begin = time.monotonic()
    for pieces, text in inputs:
        result = subprocess.run(
            [EXE, "0", "256", "100000", "-1", *map(str, args)],
            input=text, text=True, capture_output=True, check=True)
        densities.append(validate(pieces, result.stdout)[0])
    print(name, "mean", sum(densities) / len(densities), "density",
          ",".join(f"{d:.6f}" for d in densities),
          "seconds", round(time.monotonic() - begin, 2), flush=True)
