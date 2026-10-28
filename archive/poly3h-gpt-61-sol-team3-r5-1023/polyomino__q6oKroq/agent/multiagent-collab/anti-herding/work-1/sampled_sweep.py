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
    cases = [checks.generate(152711 + i, n, "uniform") for i in range(5)]
    for beam, probes in [(1, 1200), (2, 600), (2, 1200), (2, 2400), (4, 1200), (4, 2400)]:
        scores, times = [], []
        for pieces in cases:
            args = [binary, "20", "1200", "10", "-3", "3", "0", str(beam), "12",
                    "1", "10", "1", "40", "25", "1", "0", "0", "0", "0",
                    "2", "100", str(probes)]
            start = time.monotonic()
            result = subprocess.run(args, input=checks.encode(pieces), capture_output=True,
                                    text=True, check=True, timeout=12)
            times.append(time.monotonic() - start)
            score, _ = checks.validate(pieces, result.stdout)
            scores.append(score)
        print("n", n, "beam", beam, "probes", probes, "density", statistics.mean(scores),
              "min", min(scores), "mean_time", statistics.mean(times), "max_time", max(times),
              flush=True)
