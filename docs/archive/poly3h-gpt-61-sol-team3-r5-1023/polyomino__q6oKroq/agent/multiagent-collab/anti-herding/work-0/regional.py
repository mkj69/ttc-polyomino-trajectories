import importlib.util
import math
import random
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "check", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py"
)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)
binary, low, high, count, seed = sys.argv[1:6]
low, high, count, seed = map(int, (low, high, count, seed))
policies = [p.split(",") for p in sys.argv[6].split(";")]
scores = [[] for _ in policies]
times = [[] for _ in policies]
rng = random.Random(seed)
for j in range(count):
    n = min(high, int(math.exp(rng.uniform(math.log(low), math.log(high + 1)))))
    pieces = check.generate(seed + j, n, "uniform")
    data = check.encode(pieces)
    for i in [(j + k) % len(policies) for k in range(len(policies))]:
        start = time.monotonic()
        output = subprocess.run(
            [binary, "portfolio"] + policies[i],
            input=data, text=True, capture_output=True, timeout=5, check=True
        )
        times[i].append(time.monotonic() - start)
        score, _ = check.validate(pieces, output.stdout)
        scores[i].append(score)
    print(n, [round(s[-1], 6) for s in scores], flush=True)
for p, s, t in zip(policies, scores, times):
    wins = sum(a > b for a, b in zip(s, scores[0]))
    print(p, "mean", statistics.mean(s), "wins", wins, "max_time", max(t), flush=True)
print("oracle", statistics.mean(max(s[j] for s in scores) for j in range(count)), flush=True)
