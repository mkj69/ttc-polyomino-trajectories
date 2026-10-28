import importlib.util
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
baseline, candidate = sys.argv[1:3]
for n in (1100, 1300, 1600, 2000):
    totals = [[] for _ in range(5)]
    timings = [[] for _ in range(5)]
    wins = [0] * 5
    for seed in range(120510, 120516):
        pieces = checks.generate(seed, n, "uniform")
        data = checks.encode(pieces)
        configs = [(baseline, [])] + [
            (candidate, ["20", "1200", "10", "-3", "3", "0",
                         str(beam), "12", "1", "10", "1", "40", "25", "1",
                         "-5", str(beam), "180", "2000"])
            for beam in (4, 6, 8, 12)]
        for i, (binary, args) in enumerate(configs):
            start = time.monotonic()
            output = subprocess.run([binary] + args, input=data, text=True, capture_output=True,
                                    check=True, timeout=8)
            timings[i].append(time.monotonic() - start)
            density, _ = checks.validate(pieces, output.stdout)
            totals[i].append(density)
            wins[i] += density > totals[0][-1]
    for i, label in enumerate(("full-baseline", "B4+exact", "B6+exact", "B8+exact", "B12+exact")):
        print(n, label, "density", statistics.mean(totals[i]), "wins", wins[i],
              "mean_time", statistics.mean(timings[i]), "max_time", max(timings[i]), flush=True)
