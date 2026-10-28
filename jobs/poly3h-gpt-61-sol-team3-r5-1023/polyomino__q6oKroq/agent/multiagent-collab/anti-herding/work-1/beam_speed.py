import importlib.util
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
for n, width, beam in [(100, 12, 64), (200, 20, 48), (400, 24, 24), (800, 24, 16)]:
    times = [[], []]
    for seed in range(89410, 89430):
        pieces = checks.generate(seed, n, "uniform")
        outputs = []
        for index in range(2):
            args = [sys.argv[1 + index], str(width), "1200", "10", "-3", "3", "0",
                    str(beam), "12", "1", "10", "1", "0", "25", "0"]
            start = time.monotonic()
            proc = subprocess.run(args, input=checks.encode(pieces), capture_output=True,
                                  text=True, check=True, timeout=5)
            times[index].append(time.monotonic() - start)
            checks.validate(pieces, proc.stdout)
            outputs.append(proc.stdout)
        assert outputs[0] == outputs[1], (n, seed)
    print("n", n, "mean", [statistics.mean(t) for t in times],
          "speed_ratio", sum(times[0]) / sum(times[1]), flush=True)
