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
n = int(sys.argv[2]) if len(sys.argv) > 2 else 100
count = int(sys.argv[3]) if len(sys.argv) > 3 else 20
width = int(sys.argv[4]) if len(sys.argv) > 4 else 20
inputs = [checks.generate(67231 + j, n, "uniform") for j in range(count)]
for component in (0, 5, 10, 20, 40, 80, 160):
    scores, elapsed = [], []
    for pieces in inputs:
        start = time.monotonic()
        result = subprocess.run(
            [binary, str(width), "1200", "10", "-3", "0", str(component)],
            input=checks.encode(pieces), capture_output=True, text=True,
            check=True, timeout=5)
        elapsed.append(time.monotonic() - start)
        score, box = checks.validate(pieces, result.stdout)
        scores.append(score)
    print("component", component, "n", n, "mean", statistics.mean(scores),
          "min", min(scores), "runtime", max(elapsed), flush=True)
