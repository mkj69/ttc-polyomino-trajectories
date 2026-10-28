import importlib.util
import math
import random
import subprocess
import time
from pathlib import Path

base = Path("/logs/agent/multiagent-collab/anti-herding/work-1")
spec = importlib.util.spec_from_file_location("harness", base / "harness.py")
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)
words = iter(map(int, (base / "catalog.in").read_text().split()))
catalog = [tuple((next(words), next(words)) for _ in range(next(words)))
           for _ in range(next(words))]
start = time.monotonic()
for n in (100, 200, 333):
    for seed in range(3):
        rng = random.Random(17847 + n * 100 + seed)
        shapes = [harness.shifted(rng.choice(catalog), rng) for _ in range(n)]
        data = harness.encode(shapes)
        area = sum(map(len, shapes))
        best = 0
        successes = 0
        best_param = None
        for width in (10, 12, 14, 16, 18, 20):
            for direction in (0, 1, 8, 9):
                height = math.ceil(area / (width * .96))
                args = [str(base / "rectangle"), str(width), "150", "30",
                        "30", "2", "3", "1", str(direction), str(height)]
                result = subprocess.run(args, input=data, text=True, capture_output=True,
                                        check=True, timeout=5)
                density, w, h = harness.validate(shapes, result.stdout)
                if w * h <= width * height:
                    successes += 1
                if density > best:
                    best, best_param = density, (width, direction, height)
        print(f"n={n} seed={seed} best={best:.7f} params={best_param} "
              f"successes={successes}/24", flush=True)
print(f"elapsed={time.monotonic()-start:.3f}", flush=True)
