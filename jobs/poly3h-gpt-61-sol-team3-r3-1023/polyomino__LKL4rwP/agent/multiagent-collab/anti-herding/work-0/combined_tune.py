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
mode = sys.argv[2] if len(sys.argv) > 2 else "window"
settings = []
if mode == "window":
    settings = [(win, 37, 37, 0, 4) for win in (160, 240, 320, 480, 640, 960)]
elif mode == "score":
    settings = [(320, pw, sw, hw, 4) for pw, sw, hw in
                ((22, 0, 2), (30, 20, 2), (37, 37, 0), (40, 0, 2),
                 (30, 40, 10), (20, 40, 5), (25, 25, 5))]
elif mode == "lookahead":
    settings = [(320, 37, 37, 0, branch) for branch in (0, 2, 4, 6, 8, 12)]
else:
    raise ValueError(mode)
for n in (100, 1000, 10000):
    width = min(63, int(10 + n ** 0.5 * 0.5))
    cases = [check.generate(shapes, n, seed) for seed in range(1, 6 if n < 10000 else 3)]
    data = [check.encode(pieces) for pieces in cases]
    for window, pw, sw, hw, branches in settings:
        densities = []
        durations = []
        for seed, (pieces, text) in enumerate(zip(cases, data)):
            args = [width, window, pw, sw, hw, 712367 + seed * 6733, branches, 10]
            start = time.monotonic()
            result = subprocess.run([binary, *map(str, args)], input=text, text=True,
                                    capture_output=True, check=True, timeout=8)
            durations.append(time.monotonic() - start)
            densities.append(check.validate(pieces, result.stdout)[0])
        print(n, (window, pw, sw, hw, branches), "density", sum(densities) / len(densities),
              "time", sum(durations) / len(durations), flush=True)
