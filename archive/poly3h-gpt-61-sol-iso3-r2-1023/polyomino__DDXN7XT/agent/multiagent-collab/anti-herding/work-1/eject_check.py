import subprocess
import time
from validate import decode, validate


for n in [100, 300, 1000, 3000]:
    data = subprocess.run(["./uniform", str(n), "11"], capture_output=True, text=True, check=True).stdout
    pieces = decode(data)
    width = {100: 14, 300: 20, 1000: 24, 3000: 48}[n]
    for attempts in [0, 500, 3000, 15000]:
        start = time.monotonic()
        output = subprocess.run(["./solution", str(width), "1", "400", str(attempts)], input=data,
                                text=True, capture_output=True, timeout=30)
        assert output.returncode == 0, output.stderr
        elapsed = time.monotonic() - start
        density = validate(pieces, output.stdout)
        print(f"n={n} width={width} attempts={attempts} density={density:.6f} time={elapsed:.3f}", flush=True)
