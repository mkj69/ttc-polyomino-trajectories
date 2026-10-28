import subprocess
import time
from pathlib import Path
from validate import decode, validate


data = Path("uniform.in").read_text()
pieces = decode(data)
results = []
for width in [24, 32, 48]:
    for sample in [8, 16, 24]:
        for contact in [4]:
            start = time.monotonic()
            output = subprocess.run(["./solution", str(width), str(sample), str(contact), "2"], input=data,
                                    text=True, capture_output=True, timeout=60)
            assert output.returncode == 0, output.stderr
            elapsed = time.monotonic() - start
            density = validate(pieces, output.stdout)
            results.append((density, elapsed, width, sample, contact))
            print(f"w={width} pool={sample} contact={contact} density={density:.6f} time={elapsed:.3f}", flush=True)
print("best", sorted(results, reverse=True)[:5])
