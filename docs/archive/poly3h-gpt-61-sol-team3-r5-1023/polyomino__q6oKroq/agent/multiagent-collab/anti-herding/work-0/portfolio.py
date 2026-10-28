import importlib.util
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "check", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py"
)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)
binary, n, count, seed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
policies = [p.split(",") for p in sys.argv[5].split(";")]
scores = [[] for p in policies]
times = [[] for p in policies]
for s in range(seed, seed + count):
    pieces = check.generate(s, n, "uniform")
    data = check.encode(pieces)
    for i, policy in enumerate(policies):
        start = time.monotonic()
        result = subprocess.run([binary, "portfolio"] + policy, input=data, text=True,
                                capture_output=True, timeout=4, check=True)
        elapsed = time.monotonic() - start
        score, rect = check.validate(pieces, result.stdout)
        scores[i].append(score)
        times[i].append(elapsed)
    if count <= 5:
        print("seed", s, "scores", [round(v[-1], 6) for v in scores], flush=True)
for p, s, t in zip(policies, scores, times):
    print(f"{p}: mean={sum(s)/count:.8f} min={min(s):.8f} max_time={max(t):.4f}", flush=True)
oracle = sum(max(scores[i][j] for i in range(len(policies))) for j in range(count)) / count
print(f"oracle={oracle:.8f}", flush=True)
