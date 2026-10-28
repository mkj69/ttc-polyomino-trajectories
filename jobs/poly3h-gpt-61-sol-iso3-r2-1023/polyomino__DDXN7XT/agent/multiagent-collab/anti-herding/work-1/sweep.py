import subprocess
import time
from validate import generate, encode, validate


pieces = generate(10000, 68173)
data = encode(pieces)
results = []
for width in [36, 64, 96]:
    for sample in [8, 24]:
        for penalty in [0, 1, 2, 3, 4]:
            start = time.monotonic()
            output = subprocess.run(["./solution", str(width), str(sample), str(penalty)], input=data,
                                    text=True, capture_output=True, timeout=20)
            assert output.returncode == 0, output.stderr
            elapsed = time.monotonic() - start
            density = validate(pieces, output.stdout)
            results.append((density, elapsed, width, sample, penalty))
            print(f"w={width} pool={sample} penalty={penalty} density={density:.6f} time={elapsed:.3f}", flush=True)
print("best", sorted(results, reverse=True)[:5])
