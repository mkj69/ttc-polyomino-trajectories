import importlib.util
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
for width in (13, 16):
    cases = [checks.generate(183011 + i, 100, "uniform") for i in range(20)]
    for noise in (0, -1, 2, 8, 24):
        scores, times = [], []
        for pieces in cases:
            args = [sys.argv[1], str(width), "1200", "10", "-3", "3", "0", "128",
                    "12", "1", "10", "1", "0", "25", "0", "4", "128", "150",
                    "2000", "2", "100", "0", "100", str(noise)]
            start = time.monotonic()
            output = subprocess.run(args, input=checks.encode(pieces), text=True,
                                    capture_output=True, timeout=8, check=True)
            times.append(time.monotonic() - start)
            density, _ = checks.validate(pieces, output.stdout)
            scores.append(density)
        print("width", width, "noise", noise, "mean", statistics.mean(scores),
              "min", min(scores), "time", statistics.mean(times), flush=True)
