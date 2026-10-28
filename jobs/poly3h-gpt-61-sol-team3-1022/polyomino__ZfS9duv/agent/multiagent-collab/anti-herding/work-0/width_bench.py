import collections
import pickle
import random
import resource
import subprocess
import sys

sys.path.insert(0, "/logs/agent/multiagent-collab/anti-herding/work-1")
from check import encode, generate, validate

ROOT = "/logs/agent/multiagent-collab/anti-herding/work-0"
with open("/logs/agent/multiagent-collab/anti-herding/work-2/catalogue.pickle", "rb") as stream:
    shapes = pickle.load(stream)
results = collections.defaultdict(list)
times = collections.defaultdict(float)
count = int(sys.argv[1]) if len(sys.argv) > 1 else 12
budget = sys.argv[2] if len(sys.argv) > 2 else ".35"
for seed in range(count):
    n = (100, 100, 100, 300, 1000, 3000)[seed % 6]
    pieces = generate(random.Random(seed + 134906), n, shapes)
    data = encode(pieces)
    row = []
    for policy in (0, 1, 2):
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        proc = subprocess.run(
            [ROOT + "/width", "0", budget, str(max(12, min(50, n // 4))), "-1", "-1", str(policy)],
            input=data, text=True, capture_output=True, check=True)
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        elapsed = after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
        times[policy] += elapsed
        density = validate(pieces, proc.stdout)
        results[policy].append(density)
        row.append((policy, round(density, 6), proc.stdout.splitlines()[0]))
    print(seed, n, row, flush=True)
print("FINAL", {p: (sum(v) / len(v), times[p]) for p, v in results.items()}, flush=True)
