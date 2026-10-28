import importlib.util
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
for n, width in ((100, 13), (1000, 24), (5000, 48), (10000, 64)):
    for probes in (1200, 2400, 100000):
        scores = [[] for _ in range(4)]
        times = [[] for _ in range(4)]
        differences = [0] * 4
        for j in range(4):
            pieces = checks.generate(267181 + j, n, "uniform")
            data = checks.encode(pieces)
            baseline = None
            for mode in range(4):
                bits = (6, 4, 6, 8)[mode]
                args = [sys.argv[1], str(width), str(probes), "10", "-3", "3", "0",
                        "0", "12", "1", "10", "1", "40", "25", "1", "0", "0",
                        "0", "2000", "2", "100", "0", "100", "2", str(bool(mode) * 1), str(bits)]
                start = time.monotonic()
                proc = subprocess.run(args, input=data, text=True, capture_output=True,
                                      check=True, timeout=15)
                times[mode].append(time.monotonic() - start)
                scores[mode].append(checks.validate(pieces, proc.stdout)[0])
                if mode == 0:
                    baseline = proc.stdout
                else:
                    differences[mode] += proc.stdout != baseline
        print("n", n, "probes", probes, "density", [statistics.mean(s) for s in scores],
              "time", [statistics.mean(t) for t in times], "differences", differences, flush=True)
