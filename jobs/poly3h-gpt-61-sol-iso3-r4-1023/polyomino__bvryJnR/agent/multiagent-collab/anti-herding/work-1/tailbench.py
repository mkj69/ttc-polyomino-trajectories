import ast
import argparse
import pathlib
import pickle
import random
import subprocess

tree = ast.parse(pathlib.Path("test.py").read_text())
definitions = [node for node in tree.body if isinstance(node, (ast.Import, ast.ImportFrom, ast.FunctionDef))]
namespace = {}
exec(compile(ast.Module(body=definitions, type_ignores=[]), "test.py", "exec"), namespace)
validate = namespace["validate"]
catalogue = pickle.loads(pathlib.Path("catalogue.pickle").read_bytes())
parser = argparse.ArgumentParser()
parser.add_argument("--sizes", default="100,150,250,400,1000")
parser.add_argument("--seeds", default="143,925,2026,9173")
parser.add_argument("--budget", default=330, type=int)
parser.add_argument("--attempts", default=40, type=int)
parser.add_argument("--nodes", default=15000, type=int)
parser.add_argument("--modes", default="-1,0,1,2")
options = parser.parse_args()
for n in map(int, options.sizes.split(",")):
    for seed in map(int, options.seeds.split(",")):
        rng = random.Random(seed)
        pieces = [rng.choice(catalogue) for _ in range(n)]
        data = str(n) + "\n"
        for piece in pieces:
            data += str(len(piece)) + "\n" + "".join(f"{x} {y}\n" for x, y in piece)
        pathlib.Path("tailbench.in").write_text(data)
        initial = subprocess.run(["./solution", "-1", "-1", "10000", "0"],
                                 input=data, capture_output=True, text=True, check=True)
        validate(pieces, initial.stdout)
        pathlib.Path("tailbench.out").write_text(initial.stdout)
        for mode in map(int, options.modes.split(",")):
            extra = [str(options.attempts), str(options.nodes), str(max(0, mode)), "1", str(options.budget)]
            run = subprocess.run(["./tailbench", "tailbench.in", "tailbench.out",
                                  "0" if mode < 0 else "3", *extra],
                                 capture_output=True, text=True, check=True, timeout=30)
            density = validate(pieces, run.stdout)
            print(n, seed, mode, density, run.stderr.strip(), flush=True)
