import subprocess
import time
from validate import decode, validate


for n in [100, 1000, 10000]:
    for seed in [11, 12, 13]:
        data = subprocess.run(["./uniform", str(n), str(seed)], capture_output=True, text=True, check=True).stdout
        pieces = decode(data)
        start = time.monotonic()
        output = subprocess.run(["./solution"], input=data, text=True, capture_output=True, timeout=5)
        assert output.returncode == 0, output.stderr
        elapsed = time.monotonic() - start
        density = validate(pieces, output.stdout)
        print(f"n={n} seed={seed} density={density:.6f} time={elapsed:.3f}", flush=True)
