import collections
import pickle
import random
import resource
import subprocess
import sys
import time

sys.path.insert(0, "/logs/agent/multiagent-collab/anti-herding/work-1")
from check import encode, generate, validate

ROOT = "/logs/agent/multiagent-collab/anti-herding/work-0"
with open("/logs/agent/multiagent-collab/anti-herding/work-2/catalogue.pickle", "rb") as stream:
    shapes = pickle.load(stream)
results = collections.defaultdict(list)
times = collections.defaultdict(float)
count = int(sys.argv[1]) if len(sys.argv) > 1 else 40
for seed in range(count):
    pieces = generate(random.Random(seed + 3825), 100, shapes)
    data = encode(pieces)
    for width in (15, 25, 40):
        for weight in (0, 30, 60):
            before = resource.getrusage(resource.RUSAGE_CHILDREN)
            proc = subprocess.run(
                [ROOT + "/fillability", str(width), "0", "0", "10", str(weight)],
                input=data, text=True, capture_output=True, check=True)
            after = resource.getrusage(resource.RUSAGE_CHILDREN)
            times[weight] += after.ru_utime + after.ru_stime - before.ru_utime - before.ru_stime
            results[weight].append(validate(pieces, proc.stdout))
    if seed % 10 == 9:
        print(seed + 1, {w: (sum(v) / len(v), times[w]) for w, v in results.items()}, flush=True)
print("FINAL", {w: (sum(v) / len(v), times[w]) for w, v in results.items()}, flush=True)
