import ast
import pathlib
import random
import subprocess
import sys
import time


def load_functions(path):
    namespace = {}
    tree = ast.parse(pathlib.Path(path).read_text())
    definitions = [node for node in tree.body if isinstance(node, (ast.Import, ast.ImportFrom, ast.FunctionDef))]
    exec(compile(ast.Module(body=definitions, type_ignores=[]), path, "exec"), namespace)
    return namespace


reference = load_functions("uniform.py")
checker = load_functions("test.py")["validate"]
binary = sys.argv[1] if len(sys.argv) > 1 else "./solution"
rng = random.Random(1103)
started = time.monotonic()
for case in range(200):
    pieces = reference["generate"](rng, rng.randint(1, 4), "tiny")
    data = str(len(pieces)) + "\n"
    for piece in pieces:
        data += str(len(piece)) + "\n" + "".join(f"{x} {y}\n" for x, y in piece)
    result = subprocess.run([binary, "3", "6", "2", "-1", "3"], input=data,
                            capture_output=True, text=True, check=True, timeout=5)
    checker(pieces, result.stdout)
    w, h = map(int, result.stdout.splitlines()[0].split())
    optimum = reference["exhaustive_area"](pieces)
    assert w * h >= optimum
print("200 tiny exhaustive-reference cases valid")

shapes = [
    [(0, 0)],
    [(x, 0) for x in range(10)],
    [(x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1)],
    [(0, 0), (1, 0), (2, 0), (2, 1), (2, 2), (1, 2), (0, 2), (0, 3), (0, 4), (1, 4)],
]
for shape in shapes:
    pieces = [[(x + 10**12, y - 10**12) for x, y in shape] for _ in range(100)]
    data = str(len(pieces)) + "\n"
    for piece in pieces:
        data += str(len(piece)) + "\n" + "".join(f"{x} {y}\n" for x, y in piece)
    result = subprocess.run([binary, "3", "6", "2", "-1", "3"], input=data,
                            capture_output=True, text=True, check=True, timeout=5)
    density = checker(pieces, result.stdout)
    print("adversarial", len(shape), "cells valid, density", density)
print("safety test seconds:", time.monotonic() - started)
