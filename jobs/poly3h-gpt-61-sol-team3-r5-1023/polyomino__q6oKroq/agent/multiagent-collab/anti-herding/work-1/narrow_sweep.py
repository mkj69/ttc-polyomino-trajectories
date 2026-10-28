import importlib.util
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
for n, beam in ((100, 256), (300, 48)):
    cases = [checks.generate(204111 + i, n, "uniform") for i in range(12)]
    for width in (5, 6, 7, 8, 9, 10, 11, 13, 16):
        scores, times = [], []
        for pieces in cases:
            args = [sys.argv[1], str(width), "1200", "10", "-3", "3", "0", str(beam),
                    "12", "1", "10", "1", "0", "25", "0", "-5", str(beam),
                    "120", "2000", "2", "100", "0", "100"]
            start = time.monotonic()
            output = subprocess.run(args, input=checks.encode(pieces), text=True,
                                    capture_output=True, check=True, timeout=8)
            times.append(time.monotonic() - start)
            density, _ = checks.validate(pieces, output.stdout)
            scores.append(density)
        print("n", n, "width", width, "density", statistics.mean(scores),
              "min", min(scores), "mean_time", statistics.mean(times),
              "max_time", max(times), flush=True)
