import importlib.util
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "check", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py"
)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)
binary, n, count, seed = sys.argv[1:5]
n, count, seed = map(int, (n, count, seed))
policies = [p.split(",") if p else [] for p in sys.argv[5].split(";")]
scores = [[] for _ in policies]
times = [[] for _ in policies]
for j in range(count):
    pieces = check.generate(seed + j, n, "uniform")
    data = check.encode(pieces)
    for i in [(j + k) % len(policies) for k in range(len(policies))]:
        start = time.monotonic()
        output = subprocess.run(
            [binary] + policies[i], input=data, text=True,
            capture_output=True, check=True, timeout=8
        )
        times[i].append(time.monotonic() - start)
        score, _ = check.validate(pieces, output.stdout)
        scores[i].append(score)
        if count <= 4:
            print(seed + j, i, round(score, 7), output.stderr.strip(), flush=True)
for p, s, t in zip(policies, scores, times):
    wins = sum(a > b for a, b in zip(s, scores[0]))
    print(p, "mean", statistics.mean(s), "wins", wins,
          "mean_time", statistics.mean(t), "max_time", max(t), flush=True)
print("oracle", statistics.mean(max(s[j] for s in scores) for j in range(count)), flush=True)
