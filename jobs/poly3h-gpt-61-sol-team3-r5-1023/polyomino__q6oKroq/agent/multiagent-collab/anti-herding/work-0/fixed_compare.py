import importlib.util
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "check", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py"
)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)
binaries = sys.argv[1:3]
n, count, seed = map(int, sys.argv[3:6])
arguments = sys.argv[6:]
elapsed = [[], []]
densities = []
for index in range(count):
    pieces = check.generate(seed + index, n, "uniform")
    data = check.encode(pieces)
    outputs = [None, None]
    for i in ([0, 1] if index % 2 else [1, 0]):
        start = time.monotonic()
        result = subprocess.run([binaries[i]] + arguments, input=data, text=True,
                                capture_output=True, timeout=10, check=True)
        elapsed[i].append(time.monotonic() - start)
        outputs[i] = result.stdout
        score, rect = check.validate(pieces, result.stdout)
    assert outputs[0] == outputs[1], f"Output differs for seed {seed+index}, n={n}"
    densities.append(score)
means = [sum(t) / count for t in elapsed]
print(f"n={n} identical={count} density={sum(densities)/count:.8f} "
      f"times={means} speedup={means[0]/means[1]:.4f}", flush=True)
