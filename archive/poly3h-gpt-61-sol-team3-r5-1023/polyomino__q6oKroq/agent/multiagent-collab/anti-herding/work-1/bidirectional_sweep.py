import importlib.util
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
for n, width, beam in ((100, 13, 256), (300, 20, 48)):
    values = [[] for _ in range(7)]
    times = [[] for _ in range(7)]
    for j in range(20):
        pieces = checks.generate(228811 + j, n, "uniform")
        data = checks.encode(pieces)
        args = [sys.argv[1], str(width), "1200", "10", "-3", "3", "0", str(beam),
                "12", "1", "10", "1", "0", "25", "0", "-5", str(beam),
                "150", "2000", "2", "100", "0", "100"]
        start = time.monotonic()
        baseline = subprocess.run(args, input=data, text=True, capture_output=True,
                                  check=True, timeout=6)
        times[0].append(time.monotonic() - start)
        density, box = checks.validate(pieces, baseline.stdout)
        values[0].append(density)
        target = (box[0] * box[1] - 1) // width
        for step in range(3):
            policy = 4 + (j + step) % 3
            start = time.monotonic()
            output = subprocess.run(args + [str(target), str(policy)], input=data, text=True,
                                    capture_output=True, check=True, timeout=6)
            times[policy].append(time.monotonic() - start)
            values[policy].append(checks.validate(pieces, output.stdout)[0])
    for policy in (0, 4, 5, 6):
        print("n", n, "policy", policy, "density", statistics.mean(values[policy]),
              "time", statistics.mean(times[policy]),
              "wins", sum(a < b for a, b in zip(values[0], values[policy])),
              "losses", sum(a > b for a, b in zip(values[0], values[policy])), flush=True)
