import subprocess
import time
from validate import decode, validate


for n in [100, 300, 1000]:
    for seed in [11, 12, 13]:
        data = subprocess.run(["./uniform", str(n), str(seed)], capture_output=True, text=True, check=True).stdout
        pieces = decode(data)
        width = {100: 14, 300: 20, 1000: 24}[n]
        for count in [48, 72, 100, 144]:
            start = time.monotonic()
            command = ["./solution", str(width), "0", "400", "0"]
            if count:
                command.extend([str(count), "16000", "10000"])
            output = subprocess.run(command, input=data, text=True, capture_output=True, timeout=5)
            assert output.returncode == 0, output.stderr
            elapsed = time.monotonic() - start
            density = validate(pieces, output.stdout)
            print(f"n={n} seed={seed} count={count} density={density:.6f} time={elapsed:.3f}", flush=True)
