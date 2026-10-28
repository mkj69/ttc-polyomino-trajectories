import importlib.util
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
binary = sys.argv[1]
for n in (1000, 10000):
    cases = [checks.generate(78310 + i, n, "uniform") for i in range(8)]
    width = 32 if n == 1000 else 64
    for cutoff, cavity in [(c, w) for c in (0, 14, 15, 16) for w in (40, 80, 120)]:
        scores, times = [], []
        for pieces in cases:
            args = [binary, str(width), "1200", "10", "-3", "3", "0",
                    "0", "12", "1", "10", "1", str(cavity), "25", "1", str(cutoff)]
            start = time.monotonic()
            result = subprocess.run(args, input=checks.encode(pieces), capture_output=True,
                                    text=True, check=True, timeout=5)
            times.append(time.monotonic() - start)
            score, box = checks.validate(pieces, result.stdout)
            scores.append(score)
        print("n", n, "cut", cutoff, "cavity", cavity, "density", statistics.mean(scores),
              "min", min(scores), "max_time", max(times), flush=True)
