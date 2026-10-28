import subprocess
import time
from validate import decode, validate


for n in [100, 300, 1000]:
    for seed in [11, 12, 13, 14, 15]:
        data = subprocess.run(["./uniform", str(n), str(seed)], capture_output=True, text=True, check=True).stdout
        pieces = decode(data)
        width = {100: 14, 300: 20, 1000: 24}[n]
        for triple in [False, True]:
            start = time.monotonic()
            command = ["./solution", str(width), "1", "400", "3000"]
            if triple:
                command.append("1")
            output = subprocess.run(command, input=data, text=True, capture_output=True, timeout=30)
            assert output.returncode == 0, output.stderr
            elapsed = time.monotonic() - start
            density = validate(pieces, output.stdout)
            print(f"n={n} seed={seed} triple={triple} density={density:.6f} time={elapsed:.3f}", flush=True)
