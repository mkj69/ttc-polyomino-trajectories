import ast
import pathlib
import pickle
import random
import subprocess
import sys
import time

namespace = {}
tree = ast.parse(pathlib.Path("uniform.py").read_text())
definitions = [node for node in tree.body if isinstance(node, (ast.Import, ast.ImportFrom, ast.FunctionDef))]
exec(compile(ast.Module(body=definitions, type_ignores=[]), "uniform.py", "exec"), namespace)
catalogue = namespace["catalogue"]
validate = namespace["run"]

cache = pathlib.Path("catalogue.pickle")
if cache.exists():
    shapes = pickle.loads(cache.read_bytes())
else:
    shapes = catalogue()
    cache.write_bytes(pickle.dumps(shapes))

seeds = (143, 925, 2026)
cases = []
for n in (100, 300, 1000, 10000):
    for seed in seeds[:1 if n >= 1000 else 3]:
        rng = random.Random(seed)
        selected = [rng.choice(shapes) for _ in range(n)]
        data = str(n) + "\n"
        for cells in selected:
            data += str(len(cells)) + "\n" + "".join(f"{x} {y}\n" for x, y in cells)
        cases.append((n, data))

configs = [tuple(map(int, value.split(","))) for value in sys.argv[1:]]
if not configs:
    configs = [(difficulty, lookahead, 12) for difficulty in (0, 3, 6, 10) for lookahead in (0, 3, 6)]
for config in configs:
    densities = {}
    times = []
    for n, data in cases:
        start = time.monotonic()
        result = subprocess.run(["./solution", *map(str, config)], input=data, text=True,
                                capture_output=True, check=True, timeout=5)
        times.append(time.monotonic() - start)
        w, h = map(int, result.stdout.splitlines()[0].split())
        total = 0
        lines = data.splitlines()
        at = 1
        for _ in range(n):
            k = int(lines[at])
            total += k
            at += k + 1
        densities.setdefault(n, []).append(total / (w * h))
    report = " ".join(f"n{n}={sum(values)/len(values):.5f}" for n, values in densities.items())
    print(config, report, f"max={max(times):.3f}s", flush=True)
