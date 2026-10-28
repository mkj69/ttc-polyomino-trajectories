import importlib.util
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "peer_check", "/logs/agent/multiagent-collab/anti-herding/work-1/check.py")
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)
shapes = check.library()
binary = sys.argv[1]
retrieve = len(sys.argv) > 2 and sys.argv[2] == "retrieve"
for n in (100, 1000, 10000):
    width = min(63, int(10 + n ** 0.5 * 0.5))
    cases = [check.generate(shapes, n, seed) for seed in range(1, 11 if n == 100 else 4 if n < 10000 else 3)]
    data = [check.encode(pieces) for pieces in cases]
    for weight in ((0, 1) if retrieve else (0, 50, 100, 200, 400)):
        densities = []
        durations = []
        for seed, (pieces, text) in enumerate(zip(cases, data)):
            args = [width, 320, 37, 37, 0, 712367 + seed * 6733, 12, 10, weight]
            if retrieve:
                args[-1] = 200
                args.append(weight)
            start = time.monotonic()
            result = subprocess.run([binary, *map(str, args)], input=text, text=True,
                                    capture_output=True, check=True, timeout=8)
            durations.append(time.monotonic() - start)
            densities.append(check.validate(pieces, result.stdout)[0])
        print(n, weight, "density", sum(densities) / len(densities),
              "time", sum(durations) / len(durations), flush=True)
