import random
import subprocess
import time
from pathlib import Path

from harness import encode, shifted, validate

root = Path("/logs/agent/multiagent-collab/anti-herding/work-1")
words = iter(map(int, (root/"catalog.in").read_text().split()))
catalog = [tuple((next(words),next(words)) for _ in range(next(words)))
           for _ in range(next(words))]
times = [0,0]
totals = [0,0]
mismatches = 0
for n in (100,333,700):
    for seed in range(3):
        rng = random.Random(17847+n*100+seed)
        shapes = [shifted(rng.choice(catalog),rng) for _ in range(n)]
        data = encode(shapes)
        for width in (12,16,24):
            outputs = []
            density = []
            for i,binary in enumerate(("flood_driver","heap_driver")):
                args = [str(root/binary),str(width),"8","64" if n < 700 else "0","7"]
                start = time.monotonic()
                result = subprocess.run(args,input=data,capture_output=True,text=True,
                                        check=True,timeout=8)
                times[i] += time.monotonic()-start
                density.append(validate(shapes,result.stdout)[0])
                totals[i] += density[-1]
                outputs.append(result.stdout)
            mismatches += outputs[0] != outputs[1]
            print(f"n={n} seed={seed} width={width} same={outputs[0]==outputs[1]} "
                  f"densities={density}",flush=True)
print(f"mismatches={mismatches}/27 runtimes={times} means={[v/27 for v in totals]}",flush=True)
