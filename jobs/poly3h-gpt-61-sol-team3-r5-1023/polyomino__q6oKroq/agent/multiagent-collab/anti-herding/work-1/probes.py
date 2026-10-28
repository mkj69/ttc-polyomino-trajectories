import importlib.util
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
for n, width in ((5000, 48), (10000, 64)):
    cases = [checks.generate(243811 + j, n, "uniform") for j in range(8)]
    for probes in (300, 600, 1200, 2400, 4800, 100000):
        values, times = [], []
        for pieces in cases:
            start = time.monotonic()
            out = subprocess.run([sys.argv[1], str(width), str(probes), "10", "-3", "3",
                                  "0", "0", "12", "1", "10", "1", "40", "25", "1"],
                                 input=checks.encode(pieces), text=True, capture_output=True,
                                 check=True, timeout=10)
            times.append(time.monotonic() - start)
            values.append(checks.validate(pieces, out.stdout)[0])
        print("n", n, "probes", probes, "density", statistics.mean(values),
              "time", statistics.mean(times), "max_time", max(times), flush=True)
