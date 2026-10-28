import subprocess
import time
from validate import decode, validate


for n in [100, 300]:
    for seed in range(10, 20):
        data = subprocess.run(["./uniform", str(n), str(seed)], capture_output=True, text=True, check=True).stdout
        pieces = decode(data)
        width = {100: 14, 300: 20}[n]
        for variant in [16, 18, 48]:
            start = time.monotonic()
            command = ["./solution", str(width), "0", "400", "0", str(variant)]
            output = subprocess.run(command, input=data, text=True, capture_output=True, timeout=5)
            assert output.returncode == 0, output.stderr
            elapsed = time.monotonic() - start
            density = validate(pieces, output.stdout)
            print(f"n={n} seed={seed} variant={variant} density={density:.6f} time={elapsed:.3f}", flush=True)
