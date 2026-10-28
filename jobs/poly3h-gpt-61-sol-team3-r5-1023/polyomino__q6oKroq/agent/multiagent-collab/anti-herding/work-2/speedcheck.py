import importlib.util
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "c", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
directory = "/logs/agent/multiagent-collab/anti-herding/work-2/"
binaries = sys.argv[1:3] if len(sys.argv) == 3 else ["odd9882", "fastbeam"]
for n, count, beam in [(100, 30, 64), (300, 15, 48), (1000, 8, 8)]:
    elapsed = [[], []]
    identical = 0
    densities = [[], []]
    args = f"16 1200 10 -3 3 0 {beam} 12 1 10 1 0 25 0".split()
    for seed in range(1201, 1201+count):
        pieces = c.generate(seed, n, "uniform")
        inp = c.encode(pieces)
        output = [None, None]
        for i in [0, 1] if seed % 2 else [1, 0]:
            start = time.monotonic()
            proc = subprocess.run([directory+binaries[i]]+args, input=inp,
                                  text=True, capture_output=True, check=True)
            elapsed[i].append(time.monotonic()-start)
            densities[i].append(c.validate(pieces, proc.stdout)[0])
            output[i] = proc.stdout
        identical += output[0] == output[1]
    means = [sum(x)/len(x) for x in elapsed]
    print(n, "times", means, "speedup", means[0]/means[1],
          "identical", identical, "/", count, "density",
          [sum(x)/len(x) for x in densities], flush=True)
