import subprocess
import time
from validate import encode, validate


shapes = {
    "comb10": [(x, 0) for x in range(7)] + [(0, 1), (2, 1), (4, 1)],
    "ring8": [(x, y) for x in range(3) for y in range(3) if (x, y) != (1, 1)],
    "line10": [(x, 0) for x in range(10)],
    "square9": [(x, y) for x in range(3) for y in range(3)],
    "stair10": [(x, y) for x in range(5) for y in [x, x + 1]],
    "cross9": [(x, 2) for x in range(5)] + [(2, y) for y in range(5) if y != 2],
    "u9": [(x, 0) for x in range(5)] + [(0, 1), (0, 2), (4, 1), (4, 2)],
}
for name, shape in shapes.items():
    pieces = [[(x - 5000, y + 8000) for x, y in shape]] * 10000
    start = time.monotonic()
    result = subprocess.run(["./solution"], input=encode(pieces), capture_output=True, text=True, timeout=5)
    elapsed = time.monotonic() - start
    assert result.returncode == 0, result.stderr
    density = validate(pieces, result.stdout)
    print(f"{name} density={density:.6f} runtime={elapsed:.3f}", flush=True)
