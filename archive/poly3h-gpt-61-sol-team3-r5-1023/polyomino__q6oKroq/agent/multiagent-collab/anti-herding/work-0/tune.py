import importlib.util
import itertools
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "peer_check", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py"
)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)

binary = sys.argv[1]
n = int(sys.argv[2]) if len(sys.argv) > 2 else 100
count = int(sys.argv[3]) if len(sys.argv) > 3 else 10
mode = sys.argv[4] if len(sys.argv) > 4 else "grid"
cases = []
for seed in range(count):
    pieces = check.generate(seed + 1, n, "uniform")
    cases.append((pieces, check.encode(pieces)))

if mode == "grid":
    params = itertools.product([16, 20, 24, 32, 48, 64], [1200], [10], [-3, 0, 3, 6], [3, 10])
else:
    params = [tuple(map(int, p.split(","))) for p in mode.split(";")]
result = []
best = [0.0] * count
for param in params:
    scores = []
    times = []
    for i, (pieces, inp) in enumerate(cases):
        start = time.monotonic()
        output = subprocess.check_output([binary] + list(map(str, param)), input=inp.encode(), timeout=10)
        times.append(time.monotonic() - start)
        score, rect = check.validate(pieces, output)
        scores.append(score)
        best[i] = max(best[i], score)
    row = (sum(scores) / count, max(times), param)
    result.append(row)
    print(f"{param}: density={row[0]:.6f} time={row[1]:.4f}", flush=True)
print("Best parameter sets:")
for row in sorted(result, reverse=True)[:12]:
    print(row)
print(f"Combined={sum(best)/count:.6f}")
