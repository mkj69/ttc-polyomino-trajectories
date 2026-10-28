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
for n in (5000, 10000):
    cases = [checks.generate(172911 + i, n, "uniform") for i in range(10)]
    configurations = [(0, 0, 64)] + [
        (tail, beam, width)
        for tail in (500,)
        for beam in (8, 16)
        for width in (32, 48)]
    for tail, beam, width in configurations:
        scores, times = [], []
        for pieces in cases:
            args = ([binary, str(width), "1200", "10", "-3", "3", "0", "0", "12",
                     "1", "10", "1", "40", "25", "1"] if not tail else
                    [binary, "hybrid", str(width), str(tail), str(beam), "150", "2000"])
            start = time.monotonic()
            result = subprocess.run(args, input=checks.encode(pieces), capture_output=True,
                                    text=True, check=True, timeout=12)
            times.append(time.monotonic() - start)
            density, _ = checks.validate(pieces, result.stdout)
            scores.append(density)
        print("n", n, "tail", tail, "beam", beam, "width", width,
              "density", statistics.mean(scores), "min", min(scores),
              "mean_time", statistics.mean(times), "max_time", max(times), flush=True)
