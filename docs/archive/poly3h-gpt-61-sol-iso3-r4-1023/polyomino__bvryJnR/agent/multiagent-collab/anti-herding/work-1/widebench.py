import argparse
import ast
import pathlib
import pickle
import random
import subprocess
import time

namespace = {}
tree = ast.parse(pathlib.Path("test.py").read_text())
definitions = [node for node in tree.body if isinstance(node, (ast.Import, ast.ImportFrom, ast.FunctionDef))]
exec(compile(ast.Module(body=definitions, type_ignores=[]), "test.py", "exec"), namespace)
validate = namespace["validate"]
shapes = pickle.loads(pathlib.Path("catalogue.pickle").read_bytes())
parser = argparse.ArgumentParser()
parser.add_argument("--modes", default="0,1")
parser.add_argument("--sizes", default="100,150,250")
parser.add_argument("--seeds", default="143,925,2026,9173")
parser.add_argument("--binary", default="./solution")
parser.add_argument("--breadth", default=64, type=int)
parser.add_argument("--widths", default="11,16,20,24,28")
parser.add_argument("--budget", default=1600, type=int)
parser.add_argument("--warm", default=20, type=int)
parser.add_argument("--branches", default=24, type=int)
parser.add_argument("--adaptive", default=0, type=int)
parser.add_argument("--tail", default=0, type=int)
parser.add_argument("--debug", action="store_true")
options = parser.parse_args()
for n in map(int, options.sizes.split(",")):
    report = {}
    for seed in map(int, options.seeds.split(",")):
        rng = random.Random(seed)
        pieces = [rng.choice(shapes) for _ in range(n)]
        data = str(n) + "\n"
        for piece in pieces:
            data += str(len(piece)) + "\n" + "".join(f"{x} {y}\n" for x, y in piece)
        for width in map(int, options.widths.split(",")):
            for mode in map(int, options.modes.split(",")):
                arguments = [-1, 6 if n >= 200 else 3, 10000, options.tail, 15000, 34, 10, 1,
                             options.warm, options.breadth, options.branches, 10, 3, options.budget,
                             0, 0, 8, 0, 10000, 0, 64, 150, 1, 128, options.adaptive,
                             4, 2, 1, mode, width]
                started = time.monotonic()
                result = subprocess.run([options.binary, *map(str, arguments)], input=data, text=True,
                                        capture_output=True, check=True, timeout=10)
                density = validate(pieces, result.stdout)
                elapsed = time.monotonic() - started
                report.setdefault((width, mode), []).append((density, elapsed))
                print(f"n={n} seed={seed} width={width} mode={mode} density={density:.6f} "
                      f"seconds={elapsed:.3f}", flush=True)
                if options.debug:
                    print(result.stderr, flush=True)
    print("MEAN", n, {p: tuple(round(sum(x[i] for x in rows) / len(rows), 6)
                              for i in range(2)) for p, rows in report.items()}, flush=True)
