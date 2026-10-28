import importlib.util
import os
import re
import statistics
import subprocess
import sys
import time

spec = importlib.util.spec_from_file_location(
    "checks", "/logs/agent/multiagent-collab/anti-herding/work-2/check.py")
checks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checks)
for n, width, beam in ((100, 13, 128), (300, 20, 48)):
    values = [[] for _ in range(4)]
    times = [[] for _ in range(4)]
    nodes = [0] * 4
    pruned = [0] * 4
    checked = [0] * 4
    env = dict(os.environ, EXACT_DEBUG="1")
    for j in range(20):
        pieces = checks.generate(215011 + j, n, "uniform")
        data = checks.encode(pieces)
        for step in range(4):
            mode = (j + step) % 4
            args = [sys.argv[1], str(width), "1200", "10", "-3", "3", "0",
                    str(beam), "12", "1", "10", "1", "0", "25", "0", "-5",
                    str(beam), "120", "2000", "2", "100", "0", "100", str(mode)]
            start = time.monotonic()
            output = subprocess.run(args, input=data, text=True, capture_output=True,
                                    check=True, timeout=6, env=env)
            times[mode].append(time.monotonic() - start)
            values[mode].append(checks.validate(pieces, output.stdout)[0])
            nodes[mode] += sum(map(int, re.findall(r"nodes=(\d+)", output.stderr)))
            pruned[mode] += sum(map(int, re.findall(r"pruned=(\d+)", output.stderr)))
            checked[mode] += sum(map(int, re.findall(r"colors=(\d+)", output.stderr)))
    for mode in range(4):
        print("n", n, "mode", mode, "density", statistics.mean(values[mode]),
              "time", statistics.mean(times[mode]), "nodes", nodes[mode],
              "checks", checked[mode], "pruned", pruned[mode],
              "wins", sum(a < b for a, b in zip(values[0], values[mode])), flush=True)
