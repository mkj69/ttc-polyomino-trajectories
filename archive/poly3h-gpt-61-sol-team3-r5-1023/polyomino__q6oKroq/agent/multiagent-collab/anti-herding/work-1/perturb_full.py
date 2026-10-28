import importlib.util
import random
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
scores = [[] for _ in range(5)]
timings = [[] for _ in range(5)]
for j in range(20):
    n = random.Random(194011 + j).randint(100, 140)
    pieces = checks.generate(194011 + j, n, "uniform")
    data = checks.encode(pieces)
    for step in range(5):
        policy = (j + step) % 5
        args = [sys.argv[1], "portfolio", "256", "0", "1300", "0", "100", "0", "100",
                str(policy)]
        start = time.monotonic()
        output = subprocess.run(args, input=data, text=True, capture_output=True,
                                check=True, timeout=6)
        timings[policy].append(time.monotonic() - start)
        density, _ = checks.validate(pieces, output.stdout)
        scores[policy].append(density)
for policy in range(5):
    print("policy", policy, "density", statistics.mean(scores[policy]),
          "wins", sum(a < b for a, b in zip(scores[0], scores[policy])),
          "time", statistics.mean(timings[policy]), flush=True)
