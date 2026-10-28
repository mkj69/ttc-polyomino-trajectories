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
    cases = [checks.generate(108410 + i, n, "uniform") for i in range(10)]
    width = 32 if n == 1000 else 64
    for lookahead, cutoff in [(0, 0), (2, 100), (2, 150), (2, 250), (4, 150)]:
        scores, times = [], []
        for pieces in cases:
            args = [binary, str(width), "1200", "10", "-3", "3", str(lookahead),
                    "0", "12", "1", "10", "1", "40", "25", "1", "0", "0", "0", "0", str(cutoff)]
            start = time.monotonic()
            result = subprocess.run(args, input=checks.encode(pieces), capture_output=True,
                                    text=True, check=True, timeout=10)
            times.append(time.monotonic() - start)
            score, box = checks.validate(pieces, result.stdout)
            scores.append(score)
        print("n", n, "lookahead", lookahead, "cutoff", cutoff, "density", statistics.mean(scores),
              "min", min(scores), "max_time", max(times), flush=True)
