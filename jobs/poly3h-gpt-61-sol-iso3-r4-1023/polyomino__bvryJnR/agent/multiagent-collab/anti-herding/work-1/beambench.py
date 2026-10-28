import argparse
import ast
import pathlib
import pickle
import random
import subprocess
import sys
import time

namespace = {}
tree = ast.parse(pathlib.Path("test.py").read_text())
definitions = [node for node in tree.body if isinstance(node, (ast.Import, ast.ImportFrom, ast.FunctionDef))]
exec(compile(ast.Module(body=definitions, type_ignores=[]), "test.py", "exec"), namespace)
validate = namespace["validate"]
shapes = pickle.loads(pathlib.Path("catalogue.pickle").read_bytes())
parser = argparse.ArgumentParser()
parser.add_argument("config", nargs="*")
parser.add_argument("--debug", action="store_true")
parser.add_argument("--sizes", default="100,150")
parser.add_argument("--seeds", default="143,925,2026,9173")
parser.add_argument("--binary", default="./solution")
options = parser.parse_args()
configs = [tuple(map(int, value.split(","))) for value in options.config]
if not configs:
    configs = [(350, 512, 24, 10, order, 1450) for order in range(4)]
for config in configs:
    report = {}
    for n in map(int, options.sizes.split(",")):
        densities = []
        for seed in map(int, options.seeds.split(",")):
            rng = random.Random(seed)
            pieces = [rng.choice(shapes) for _ in range(n)]
            data = str(n) + "\n"
            for piece in pieces:
                data += str(len(piece)) + "\n" + "".join(f"{x} {y}\n" for x, y in piece)
            command = [options.binary, "-1", "-1", "10000", "80", "15000", "34", "10", "1", *map(str, config)]
            command[2] = str(6 if 200 <= n < 500 else 3)
            start = time.monotonic()
            result = subprocess.run(command, input=data, text=True, capture_output=True, check=True, timeout=5)
            density = validate(pieces, result.stdout)
            densities.append(density)
            print(config, f"n={n} seed={seed} density={density:.5f} time={time.monotonic()-start:.3f}", flush=True)
            if options.debug:
                print(result.stderr, flush=True)
        report[n] = sum(densities) / len(densities)
    print("MEAN", config, report, flush=True)
