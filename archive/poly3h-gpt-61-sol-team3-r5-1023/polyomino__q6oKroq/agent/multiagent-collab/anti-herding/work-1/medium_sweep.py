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
for n in (2500, 3000, 4000):
    cases = [checks.generate(119410 + i, n, "uniform") for i in range(4)]
    for width, beam in [(w, b) for w in (20, 24) for b in (1, 2, 4)]:
        scores, times = [], []
        for pieces in cases:
            args = [binary, str(width), "1200", "10", "-3", "3", "0",
                    str(beam), "12", "1", "10", "1", "40", "25", "1"]
            start = time.monotonic()
            result = subprocess.run(args, input=checks.encode(pieces), capture_output=True,
                                    text=True, check=True, timeout=10)
            times.append(time.monotonic() - start)
            score, box = checks.validate(pieces, result.stdout)
            scores.append(score)
        print("n", n, "width", width, "beam", beam, "density", statistics.mean(scores),
              "min", min(scores), "mean_time", statistics.mean(times),
              "max_time", max(times), flush=True)
