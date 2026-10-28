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
for n in (4200, 5000, 7000, 10000):
    scores = [[], []]
    times = [[], []]
    for j in range(8):
        data_pieces = checks.generate(142611 + j, n, "uniform")
        data = checks.encode(data_pieces)
        for i in ((j % 2), (1 - j % 2)):
            binary = (baseline, candidate)[i]
            start = time.monotonic()
            output = subprocess.run([binary], input=data, text=True,
                                    capture_output=True, check=True, timeout=6)
            times[i].append(time.monotonic() - start)
            density, _ = checks.validate(data_pieces, output.stdout)
            scores[i].append(density)
    wins = sum(a < b for a, b in zip(*scores))
    print(n, "density", [statistics.mean(s) for s in scores], "wins", wins,
          "times", [statistics.mean(t) for t in times], "max", [max(t) for t in times],
          flush=True)
