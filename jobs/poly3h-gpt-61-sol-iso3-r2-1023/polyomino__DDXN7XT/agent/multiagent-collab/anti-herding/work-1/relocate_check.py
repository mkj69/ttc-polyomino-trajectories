import subprocess
import time
from validate import decode, validate


for n in [100, 300, 1000, 3000, 10000]:
    data = subprocess.run(["./uniform", str(n), "11"], capture_output=True, text=True, check=True).stdout
    pieces = decode(data)
    width = {100: 14, 300: 20, 1000: 24, 3000: 48, 10000: 48}[n]
    for passes in [0, 1, 3, 8]:
        start = time.monotonic()
        output = subprocess.run(["./solution", str(width), str(passes)], input=data,
                                text=True, capture_output=True, timeout=30)
        assert output.returncode == 0, output.stderr
        elapsed = time.monotonic() - start
        density = validate(pieces, output.stdout)
        print(f"n={n} width={width} passes={passes} density={density:.6f} time={elapsed:.3f}", flush=True)
