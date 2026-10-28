import importlib.util
import random
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)


def transformed(shape, turn, flip, dx, dy):
    result = []
    for x, y in shape:
        if flip:
            x = -x
        for _ in range(turn):
            x, y = y, -x
        result.append((x + dx, y + dy))
    return result


ring = [(x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1)]
stair = [(i // 2, (i + 1) // 2) for i in range(10)]
hook = [(x, 0) for x in range(6)] + [(0, y) for y in range(1, 5)]
c_shape = [(0, y) for y in range(5)] + [(1, 0), (2, 0), (1, 4), (2, 4)]
cases = [
    ("single", [[(-2147483640, 2147483640)]]),
    ("ten_line", [[(i - 2147483640, 2147483640) for i in range(10)]]),
]
for name, base in [("monos", [(0, 0)]), ("line", [(i, 0) for i in range(10)]),
                   ("ring", ring), ("stair", stair), ("hook", hook), ("C", c_shape)]:
    for n in (100, 10000):
        cases.append((name + str(n), [
            transformed(base, i % 4, i // 4 % 2, -2147483640 + i % 9, 2147483600 - i % 7)
            for i in range(n)]))

for n in (100, 257, 600, 1024, 2500, 10000):
    cases.append(("uniform" + str(n), checks.generate(91150 + n, n, "uniform")))

for name, pieces in cases:
    start = time.monotonic()
    result = subprocess.run([sys.argv[1]], input=checks.encode(pieces), text=True,
                            capture_output=True, timeout=6, check=True)
    density, box = checks.validate(pieces, result.stdout)
    print(name, "density", round(density, 6), "box", box, "time",
          round(time.monotonic() - start, 4), flush=True)
