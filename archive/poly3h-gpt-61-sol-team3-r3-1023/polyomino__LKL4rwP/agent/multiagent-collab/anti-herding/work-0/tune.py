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
mode = sys.argv[2] if len(sys.argv) > 2 else "width"
if mode == "width":
    settings = [(w, 160, 37, 37, 0) for w in (8, 10, 12, 15, 18, 20, 25, 30, 40, 50, 63)]
elif mode == "score":
    settings = [(20, 160, pw, sw, hw) for pw, sw, hw in
                ((22, 0, 2), (22, 20, 2), (30, 20, 2), (37, 37, 0),
                 (40, 0, 2), (45, 20, 2), (30, 40, 10), (20, 40, 5),
                 (25, 25, 5), (35, 50, 5), (40, 50, 5))]
elif mode == "window":
    settings = [(20, win, 30, 40, 10) for win in (20, 40, 80, 120, 160, 240, 320, 640, 10000)]
else:
    raise ValueError(mode)
for n in (100, 1000, 10000):
    cases = [check.generate(shapes, n, seed) for seed in range(1, 4 if n < 10000 else 2)]
    data = [check.encode(pieces) for pieces in cases]
    for setting in settings:
        densities = []
        durations = []
        for pieces, text in zip(cases, data):
            start = time.monotonic()
            result = subprocess.run([binary, *map(str, setting)], input=text, text=True,
                                    capture_output=True, check=True, timeout=15)
            durations.append(time.monotonic() - start)
            densities.append(check.validate(pieces, result.stdout)[0])
        print(n, setting, "density", sum(densities) / len(densities),
              "time", sum(durations) / len(durations), flush=True)
