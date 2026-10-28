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
warm = sys.argv[1] if len(sys.argv) > 1 else "900"
for n in (100, 150, 300, 500):
    for seed in (143, 925, 2026):
        rng = random.Random(seed)
        pieces = [rng.choice(shapes) for _ in range(n)]
        data = str(n) + "\n"
        for piece in pieces:
            data += str(len(piece)) + "\n" + "".join(f"{x} {y}\n" for x, y in piece)
        start = time.monotonic()
        result = subprocess.run(["./solution", "-1", "3", "100000", "-1", "3", warm],
                                input=data, text=True, capture_output=True, check=True, timeout=5)
        density = validate(pieces, result.stdout)
        print(f"n={n} seed={seed} density={density:.5f} time={time.monotonic()-start:.3f}", flush=True)
        print(result.stderr.strip(), flush=True)
