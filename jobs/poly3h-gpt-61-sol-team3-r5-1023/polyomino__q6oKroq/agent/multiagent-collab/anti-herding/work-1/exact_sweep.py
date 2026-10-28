import importlib.util
import re
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
cases = [checks.generate(50830 + i, 100, "uniform") for i in range(8)]
binary = sys.argv[1]
settings = [(c, m, 0) for c in (8, 12, 20, 40) for m in (2, 12)]
for count, mode, shift in settings:
    improvements, scores, runtimes = [], [], []
    for pieces in cases:
        args = [binary, "16", "1200", "10", "-3", "3", "0", "64", "12",
                "1", "10", "1", "10", "25", "0", str(count), str(mode), "350", str(shift)]
        start = time.monotonic()
        result = subprocess.run(args, input=checks.encode(pieces), capture_output=True,
                                text=True, check=True, timeout=5)
        runtimes.append(time.monotonic() - start)
        score, box = checks.validate(pieces, result.stdout)
        match = re.search(r"initial=(\d+) final=(\d+) successes=(\d+)", result.stderr)
        assert match, result.stderr
        before, after, successes = map(int, match.groups())
        improvements.append(before - after)
        scores.append(score)
    print("count", count, "mode", mode, "shift", shift, "gain", statistics.mean(improvements),
          "density", statistics.mean(scores), "improved", sum(v > 0 for v in improvements),
          "max_time", max(runtimes), flush=True)
